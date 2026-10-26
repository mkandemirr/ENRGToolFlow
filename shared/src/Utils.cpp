#include "Utils.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

#include <TBranch.h>
#include <TFile.h>
#include <TFriendElement.h>
#include <TLeaf.h>
#include <TTree.h>

#include "Waveform.h"

namespace
{
// Reads the eventCounter branch of every entry. The caller's branch status and
// address are restored afterwards.
bool ReadEventCounters(TTree* tree, const std::string& label,
                       std::vector<UInt_t>& counters)
{
   TBranch* branch = tree->GetBranch("eventCounter");
   TLeaf* leaf = branch ? branch->GetLeaf("eventCounter") : nullptr;

   if (!leaf)
   {
      std::cerr << "Error: " << label << " has no eventCounter branch" << std::endl;
      return false;
   }

   if (std::string(leaf->GetTypeName()) != "UInt_t")
   {
      std::cerr << "Error: eventCounter in " << label << " is "
                << leaf->GetTypeName() << ", expected UInt_t" << std::endl;
      return false;
   }

   // A disabled branch is not read, even through TBranch::GetEntry.
   const bool oldStatus = tree->GetBranchStatus("eventCounter");
   tree->SetBranchStatus("eventCounter", 1);

   char* oldAddress = branch->GetAddress();
   UInt_t counter = 0;
   branch->SetAddress(&counter);

   const Long64_t nEntries = tree->GetEntries();
   counters.resize(nEntries);

   for (Long64_t entry = 0; entry < nEntries; ++entry)
   {
      branch->GetEntry(entry);
      counters[entry] = counter;
   }

   if (oldAddress)
   {
      branch->SetAddress(oldAddress);
   }
   else
   {
      tree->ResetBranchAddress(branch);
   }

   tree->SetBranchStatus("eventCounter", oldStatus);

   return true;
}
}

namespace Utils
{
BaselineResult ComputeBaseline(const Waveform& waveform, UInt_t firstSample,
                               UInt_t nSamples)
{
   BaselineResult result;
   const UInt_t end = static_cast<UInt_t>(
      std::min<ULong64_t>(static_cast<ULong64_t>(firstSample) + nSamples,
                          waveform.GetNSamples()));

   if (firstSample >= end)
   {
      return result;
   }

   double sum = 0;
   double sum2 = 0;

   for (UInt_t i = firstSample; i < end; ++i)
   {
      const double value = waveform.GetValueMv(i);
      sum += value;
      sum2 += value * value;
   }

   result.nSamples = end - firstSample;
   result.meanMv = sum / result.nSamples;
   result.rmsMv = std::sqrt(std::max(0.0, sum2 / result.nSamples
                                          - result.meanMv * result.meanMv));
   return result;
}

bool AddFriends(TTree* tree, const std::vector<std::string>& friendFiles,
                const std::vector<std::string>& friendTreeNames)
{
   if (!tree)
   {
      std::cerr << "Error: AddFriends called without a tree" << std::endl;
      return false;
   }

   if (friendFiles.size() != friendTreeNames.size())
   {
      std::cerr << "Error: " << friendFiles.size() << " friend files but "
                << friendTreeNames.size() << " friend tree names" << std::endl;
      return false;
   }

   const std::string label = std::string("tree ") + tree->GetName();
   std::vector<UInt_t> counters;

   if (!ReadEventCounters(tree, label, counters))
   {
      return false;
   }

   for (size_t i = 0; i < friendFiles.size(); ++i)
   {
      const std::string& path = friendFiles[i];
      const std::string& treeName = friendTreeNames[i];
      std::unique_ptr<TFile> file(TFile::Open(path.c_str(), "READ"));

      if (!file || file->IsZombie())
      {
         std::cerr << "Error: cannot open friend file " << path << std::endl;
         return false;
      }

      TTree* friendTree = file->Get<TTree>(treeName.c_str());

      if (!friendTree)
      {
         std::cerr << "Error: no tree '" << treeName << "' in " << path << std::endl;
         return false;
      }

      if (friendTree->GetEntries() != tree->GetEntries())
      {
         std::cerr << "Error: " << path << " has " << friendTree->GetEntries()
                   << " entries, " << label << " has " << tree->GetEntries()
                   << std::endl;
         return false;
      }

      std::vector<UInt_t> friendCounters;

      if (!ReadEventCounters(friendTree, path, friendCounters))
      {
         return false;
      }

      for (size_t entry = 0; entry < counters.size(); ++entry)
      {
         if (friendCounters[entry] != counters[entry])
         {
            std::cerr << "Error: entry " << entry << ": eventCounter is "
                      << friendCounters[entry] << " in " << path << " but "
                      << counters[entry] << " in " << label << std::endl;
            return false;
         }
      }
   }

   for (size_t i = 0; i < friendFiles.size(); ++i)
   {
      if (!tree->AddFriend(friendTreeNames[i].c_str(), friendFiles[i].c_str()))
      {
         std::cerr << "Error: cannot add friend " << friendFiles[i] << std::endl;
         return false;
      }
   }

   return true;
}
}
