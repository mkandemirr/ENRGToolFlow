#include "RawWaveformReader.h"

#include <iostream>

#include <TBranch.h>
#include <TFile.h>
#include <TLeaf.h>
#include <TTree.h>

#include "Event.h"

namespace
{
// Returns the type name of a single-leaf branch, or "" if the branch is missing.
std::string GetLeafType(TTree* tree, const char* branchName)
{
   TBranch* branch = tree->GetBranch(branchName);

   if (branch == nullptr || branch->GetListOfLeaves()->GetEntries() != 1)
   {
      return "";
   }

   return static_cast<TLeaf*>(branch->GetListOfLeaves()->At(0))->GetTypeName();
}
}

RawWaveformReader::~RawWaveformReader()
{
   delete fFile;
}

bool RawWaveformReader::Open(const std::string& fileName, const std::string& treeName,
                             Long64_t cacheSizeBytes)
{
   fFile = TFile::Open(fileName.c_str(), "READ");

   if (fFile == nullptr || fFile->IsZombie())
   {
      std::cerr << "Error: could not open input file: " << fileName << std::endl;
      return false;
   }

   fTree = dynamic_cast<TTree*>(fFile->Get(treeName.c_str()));

   if (fTree == nullptr)
   {
      std::cerr << "Error: TTree '" << treeName << "' not found in " << fileName
                << std::endl;
      return false;
   }

   // Expected ENRGDAQ branches and their types.
   const std::pair<const char*, const char*> expectedBranches[] = {
      {"real_ns_timestamp", "ULong64_t"},
      {"event_counter", "UInt_t"},
      {"channel", "UChar_t"},
      {"value_mv", "Short_t"},
   };

   for (const auto& [name, type] : expectedBranches)
   {
      const std::string foundType = GetLeafType(fTree, name);

      if (foundType != type)
      {
         std::cerr << "Error: branch '" << name << "' has type '" << foundType
                   << "', expected '" << type << "'" << std::endl;
         return false;
      }
   }

   const std::string sampleIndexType = GetLeafType(fTree, "sample_index");

   if (sampleIndexType == "UInt_t")
   {
      fSampleIndexIs32 = true;
   }
   else if (sampleIndexType != "UShort_t")
   {
      std::cerr << "Error: branch 'sample_index' has type '" << sampleIndexType
                << "', expected 'UShort_t' or 'UInt_t'" << std::endl;
      return false;
   }

   const char* usedBranches[] = {"real_ns_timestamp", "event_counter", "channel",
                                 "sample_index", "value_mv"};

   fTree->SetBranchStatus("*", 0);

   for (const char* name : usedBranches)
   {
      fTree->SetBranchStatus(name, 1);
   }

   fTree->SetBranchAddress("real_ns_timestamp", &fTimestampNs);
   fTree->SetBranchAddress("event_counter", &fEventCounter);
   fTree->SetBranchAddress("channel", &fChannel);
   fTree->SetBranchAddress("value_mv", &fValueMv);

   if (fSampleIndexIs32)
   {
      fTree->SetBranchAddress("sample_index", &fSampleIndex32);
   }
   else
   {
      fTree->SetBranchAddress("sample_index", &fSampleIndex16);
   }

   fTree->SetCacheSize(cacheSizeBytes);

   for (const char* name : usedBranches)
   {
      fTree->AddBranchToCache(name, true);
   }

   fNEntries = fTree->GetEntries();
   fNextEntry = 0;
   fHasPendingRow = false;

   return true;
}

RawWaveformReader::Status RawWaveformReader::ReadNextEvent(Event& event)
{
   event.Clear();

   if (!fHasPendingRow)
   {
      if (fNextEntry >= fNEntries)
      {
         return Status::kEndOfTree;
      }

      LoadRow(fNextEntry);
   }

   fHasPendingRow = false;

   // If the first row is valid its sample_index is 0; this is checked below.
   event.SetEventCounter(fEventCounter);
   event.SetTriggerTimeNs(fTimestampNs - fSampleIndex);
   event.AddChannel(fChannel);

   while (true)
   {
      const UInt_t expectedIndex = event.GetChannelNSamples().back();

      if (fSampleIndex != expectedIndex)
      {
         PrintRowError("sample_index is " + std::to_string(fSampleIndex)
                       + ", expected " + std::to_string(expectedIndex));
         return Status::kError;
      }

      if (fTimestampNs != event.GetTriggerTimeNs() + fSampleIndex)
      {
         PrintRowError("real_ns_timestamp differs from triggerTimeNs + sample_index"
                       " (triggerTimeNs = "
                       + std::to_string(event.GetTriggerTimeNs()) + ")");
         return Status::kError;
      }

      event.AddSample(fValueMv);

      if (fNextEntry >= fNEntries)
      {
         return Status::kEvent;
      }

      LoadRow(fNextEntry);

      if (fEventCounter != event.GetEventCounter())
      {
         fHasPendingRow = true;
         return Status::kEvent;
      }

      if (fChannel != event.GetChannelIds().back() || fSampleIndex == 0)
      {
         event.AddChannel(fChannel);
      }
   }
}

void RawWaveformReader::LoadRow(Long64_t entry)
{
   fTree->GetEntry(entry);
   fSampleIndex = fSampleIndexIs32 ? fSampleIndex32 : fSampleIndex16;
   fNextEntry = entry + 1;
}

void RawWaveformReader::PrintRowError(const std::string& message) const
{
   std::cerr << "Error: raw row " << fNextEntry - 1
             << " (event_counter " << fEventCounter
             << ", channel " << static_cast<int>(fChannel)
             << ", sample_index " << fSampleIndex
             << ", real_ns_timestamp " << fTimestampNs << "): " << message
             << ". The data cannot be stored losslessly in the events format."
             << std::endl;
}
