#include "Event.h"

#include <stdexcept>
#include <string>

#include <TTree.h>

const char* Event::GetQualityBitName(int bit)
{
   static const char* const names[kNQualityBits] = {
      "missing channel",
      "duplicate channel",
      "unequal channel lengths",
      "record length at DAQ limit",
      "eventCounter gap",
      "eventCounter backward",
      "triggerTimeNs backward",
   };

   return (bit >= 0 && bit < kNQualityBits) ? names[bit] : "unknown";
}

void Event::Clear()
{
   fEventCounter = 0;
   fTriggerTimeNs = 0;
   fNChannels = 0;
   fChannelId.clear();
   fChannelNSamples.clear();
   fSampleValueMv.clear();
   fQualityFlags = 0;
}

void Event::AddChannel(UChar_t channel)
{
   fChannelId.push_back(channel);
   fChannelNSamples.push_back(0);
   fNChannels = fChannelId.size();
}

void Event::AddSample(Short_t valueMv)
{
   fSampleValueMv.push_back(valueMv);
   ++fChannelNSamples.back();
}

void Event::CreateBranches(TTree* tree)
{
   tree->Branch("eventCounter", &fEventCounter, "eventCounter/i");
   tree->Branch("triggerTimeNs", &fTriggerTimeNs, "triggerTimeNs/l");
   tree->Branch("nChannels", &fNChannels, "nChannels/i");
   tree->Branch("channelId", &fChannelId);
   tree->Branch("channelNSamples", &fChannelNSamples);
   tree->Branch("sampleValueMv", &fSampleValueMv);
   tree->Branch("qualityFlags", &fQualityFlags, "qualityFlags/i");
}

void Event::SetBranchAddresses(TTree* tree)
{
   tree->SetBranchAddress("eventCounter", &fEventCounter);
   tree->SetBranchAddress("triggerTimeNs", &fTriggerTimeNs);
   tree->SetBranchAddress("nChannels", &fNChannels);
   tree->SetBranchAddress("channelId", &fChannelIdAddress);
   tree->SetBranchAddress("channelNSamples", &fChannelNSamplesAddress);
   tree->SetBranchAddress("sampleValueMv", &fSampleValueMvAddress);
   tree->SetBranchAddress("qualityFlags", &fQualityFlags);
}

Channel Event::GetChannelAt(UInt_t k) const
{
   if (k >= fChannelId.size())
   {
      throw std::out_of_range("Event::GetChannelAt: channel block " + std::to_string(k)
                              + " does not exist");
   }

   size_t offset = 0;

   for (UInt_t i = 0; i < k; ++i)
   {
      offset += fChannelNSamples[i];
   }

   const Waveform waveform(fSampleValueMv.data() + offset, fChannelNSamples[k],
                           fTriggerTimeNs);
   return Channel(fChannelId[k], waveform);
}

std::optional<Channel> Event::GetChannel(int channelId) const
{
   for (UInt_t k = 0; k < fChannelId.size(); ++k)
   {
      if (fChannelId[k] == channelId)
      {
         return GetChannelAt(k);
      }
   }

   return std::nullopt;
}

std::vector<Channel> Event::GetChannels() const
{
   std::vector<Channel> channels;
   channels.reserve(fChannelId.size());
   size_t offset = 0;

   for (UInt_t k = 0; k < fChannelId.size(); ++k)
   {
      const Waveform waveform(fSampleValueMv.data() + offset, fChannelNSamples[k],
                              fTriggerTimeNs);
      channels.emplace_back(fChannelId[k], waveform);
      offset += fChannelNSamples[k];
   }

   return channels;
}
