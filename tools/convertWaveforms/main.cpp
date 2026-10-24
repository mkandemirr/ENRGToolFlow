#include <bitset>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <TFile.h>
#include <TTree.h>

#include "ConfigReader.h"
#include "Event.h"
#include "INIReader.h"
#include "RawWaveformReader.h"

// Returns 0 on success, non-zero on failure.
int convertWaveforms(const INIReader& iniReader);

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      std::cerr << "Usage: ./bin/convertWaveforms configs/convertWaveforms/config.ini"
                << std::endl;
      return 1;
   }

   INIReader iniReader(argv[1]);

   if (iniReader.ParseError() != 0)
   {
      std::cerr << "Error: could not load config file: "
                << argv[1] << std::endl;
      return 1;
   }

   return convertWaveforms(iniReader);
}

namespace
{
// Flags that depend only on the event itself.
UInt_t ComputeEventFlags(const Event& event,
                         const std::vector<int>& expectedChannels,
                         long long daqMaxSamplesPerChannel)
{
   UInt_t flags = 0;
   std::bitset<256> seen;

   for (UChar_t channel : event.GetChannelIds())
   {
      if (seen[channel])
      {
         flags |= Event::kDuplicateChannel;
      }

      seen[channel] = true;
   }

   for (int channel : expectedChannels)
   {
      if (!seen[channel])
      {
         flags |= Event::kMissingChannel;
      }
   }

   const std::vector<UInt_t>& nSamples = event.GetChannelNSamples();

   for (UInt_t n : nSamples)
   {
      if (n != nSamples.front())
      {
         flags |= Event::kUnequalLength;
      }

      if (n == daqMaxSamplesPerChannel)
      {
         flags |= Event::kAtDaqLimit;
      }
   }

   return flags;
}

}

int convertWaveforms(const INIReader& iniReader)
{
   // Read every key before stopping, so that all config problems are reported.
   ConfigReader config(iniReader);
   const std::string inputFileName = config.GetString("input", "rootFile");
   const std::string inputTreeName = config.GetString("input", "treeName");
   const long long readCacheSizeMb = config.GetInteger("input", "readCacheSizeMb");
   const std::string outputFileName = config.GetString("output", "rootFile");
   const std::string outputTreeName = config.GetString("output", "treeName");
   const long long compressionSetting = config.GetInteger("output", "compressionSetting");
   const bool overwriteOutput = config.GetBoolean("output", "overwriteOutput");
   const std::vector<int> expectedChannels =
      config.GetIntegerList("analysis", "expectedChannels");
   const long long daqMaxSamplesPerChannel =
      config.GetInteger("analysis", "daqMaxSamplesPerChannel");
   const long long maxEvents = config.GetInteger("analysis", "maxEvents");

   if (!config.IsValid())
   {
      return 1;
   }

   for (int channel : expectedChannels)
   {
      if (channel < 0 || channel > 255)
      {
         std::cerr << "Error: invalid channel " << channel << " in expectedChannels"
                   << std::endl;
         return 1;
      }
   }

   // Never overwrite the input file, and protect existing output.
   namespace fs = std::filesystem;
   std::error_code errorCode;

   if (fs::weakly_canonical(inputFileName, errorCode)
       == fs::weakly_canonical(outputFileName, errorCode))
   {
      std::cerr << "Error: output file is the same as the input file: "
                << outputFileName << std::endl;
      return 1;
   }

   if (fs::exists(outputFileName) && !overwriteOutput)
   {
      std::cerr << "Error: output file already exists (set overwriteOutput = true "
                << "to replace it): " << outputFileName << std::endl;
      return 1;
   }

   const fs::path outputDirectory = fs::path(outputFileName).parent_path();

   if (!outputDirectory.empty() && !fs::exists(outputDirectory))
   {
      fs::create_directories(outputDirectory, errorCode);

      if (errorCode)
      {
         std::cerr << "Error: could not create output directory "
                   << outputDirectory << ": " << errorCode.message() << std::endl;
         return 1;
      }
   }

   RawWaveformReader reader;

   if (!reader.Open(inputFileName, inputTreeName, readCacheSizeMb * 1024 * 1024))
   {
      return 1;
   }

   TFile outputFile(outputFileName.c_str(), "RECREATE", "", compressionSetting);

   if (outputFile.IsZombie())
   {
      std::cerr << "Error: could not create output file: " << outputFileName
                << std::endl;
      return 1;
   }

   TTree* outputTree = new TTree(outputTreeName.c_str(),
                                 "ENRGDAQ waveforms, one entry per event");
   Event event;
   event.CreateBranches(outputTree);

   std::cout << "Input:  " << inputFileName << " (" << reader.GetNEntries()
             << " rows)" << std::endl;
   std::cout << "Output: " << outputFileName << std::endl;

   const auto startTime = std::chrono::steady_clock::now();

   Long64_t nEvents = 0;
   Long64_t nRowsWritten = 0;
   Long64_t nMissingEvents = 0;
   Long64_t nFlaggedEvents = 0;
   Long64_t qualityBitCounts[Event::kNQualityBits] = {};
   std::map<UInt_t, Long64_t> nChannelsCounts;
   std::map<UInt_t, Long64_t> recordLengthCounts;
   UInt_t previousCounter = 0;
   ULong64_t previousTimeNs = 0;
   int nextProgressPercent = 10;
   bool readError = false;

   while (maxEvents < 0 || nEvents < maxEvents)
   {
      const RawWaveformReader::Status status = reader.ReadNextEvent(event);

      if (status == RawWaveformReader::Status::kEndOfTree)
      {
         break;
      }

      if (status == RawWaveformReader::Status::kError)
      {
         readError = true;
         break;
      }

      UInt_t flags = ComputeEventFlags(event, expectedChannels,
                                       daqMaxSamplesPerChannel);

      if (nEvents > 0)
      {
         if (event.GetEventCounter() > previousCounter + 1)
         {
            flags |= Event::kCounterGap;
            nMissingEvents += event.GetEventCounter() - previousCounter - 1;
         }
         else if (event.GetEventCounter() < previousCounter)
         {
            flags |= Event::kCounterBackward;
         }

         if (event.GetTriggerTimeNs() < previousTimeNs)
         {
            flags |= Event::kTimeBackward;
         }
      }

      event.SetQualityFlags(flags);
      outputTree->Fill();

      ++nEvents;
      nRowsWritten += event.GetSampleValuesMv().size();
      previousCounter = event.GetEventCounter();
      previousTimeNs = event.GetTriggerTimeNs();

      if (flags != 0)
      {
         ++nFlaggedEvents;
      }

      for (int bit = 0; bit < Event::kNQualityBits; ++bit)
      {
         if (flags & (1u << bit))
         {
            ++qualityBitCounts[bit];
         }
      }

      ++nChannelsCounts[event.GetNChannels()];

      for (UInt_t n : event.GetChannelNSamples())
      {
         ++recordLengthCounts[n];
      }

      const double fraction = (maxEvents > 0)
         ? static_cast<double>(nEvents) / maxEvents
         : static_cast<double>(reader.GetNEntriesRead()) / reader.GetNEntries();

      while (fraction * 100 >= nextProgressPercent && nextProgressPercent <= 100)
      {
         std::cout << "Progress: " << nextProgressPercent << "% (" << nEvents
                   << " events)" << std::endl;
         nextProgressPercent += 10;
      }
   }

   outputFile.cd();
   outputTree->Write();
   outputFile.Close();

   const double elapsedS = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - startTime).count();

   std::cout << "\n=== convertWaveforms summary ===" << std::endl;
   std::cout << "Input file:          " << inputFileName << std::endl;
   std::cout << "Output file:         " << outputFileName << std::endl;
   std::cout << "Events written:      " << nEvents << std::endl;
   std::cout << "Events skipped:      0 (flagged events are kept)" << std::endl;
   std::cout << "Raw rows written:    " << nRowsWritten << " of "
             << reader.GetNEntries() << std::endl;
   std::cout << "Flagged events:      " << nFlaggedEvents << std::endl;

   for (int bit = 0; bit < Event::kNQualityBits; ++bit)
   {
      std::cout << "  bit " << bit << " (" << Event::GetQualityBitName(bit) << "): "
                << qualityBitCounts[bit] << std::endl;
   }

   std::cout << "Missing events (counter gaps): " << nMissingEvents << std::endl;
   std::cout << "Channels per event (nChannels: events):";

   for (const auto& [nChannels, count] : nChannelsCounts)
   {
      std::cout << " " << nChannels << ": " << count;
   }

   std::cout << "\nRecord lengths (samples: channel blocks):";

   for (const auto& [length, count] : recordLengthCounts)
   {
      std::cout << " " << length << ": " << count;
   }

   std::cout << "\nElapsed time:        " << elapsedS << " s ("
             << reader.GetNEntriesRead() / elapsedS / 1e6 << " M rows/s)" << std::endl;

   if (readError)
   {
      std::cerr << "Error: conversion stopped at an inconsistent raw row; the output "
                << "file is incomplete." << std::endl;
      return 1;
   }

   return 0;
}
