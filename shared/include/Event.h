#ifndef EVENT_H
#define EVENT_H

#include <optional>
#include <vector>

#include <Rtypes.h>

#include "Channel.h"

class TTree;

// One digitizer event: all channel records of one trigger.
//
// The samples of all channels are stored one after another in a single vector
// (channel-major). Channel block k holds GetChannelNSamples()[k] samples; sample i
// of block k was recorded at GetTriggerTimeNs() + i.
//
// For reading, GetChannel() / GetChannelAt() / GetChannels() return Channel views
// into this object; they are valid until the object is modified or the next tree
// entry is read.
class Event
{
public:
   // Bits of the qualityFlags branch. They are informational: a flagged event
   // is still complete and lossless.
   static constexpr UInt_t kMissingChannel   = 1u << 0;  // expected channel absent
   static constexpr UInt_t kDuplicateChannel = 1u << 1;  // channel appears twice
   static constexpr UInt_t kUnequalLength    = 1u << 2;  // channel lengths differ
   static constexpr UInt_t kAtDaqLimit       = 1u << 3;  // length == DAQ limit
   static constexpr UInt_t kCounterGap       = 1u << 4;  // eventCounter jumped > 1
   static constexpr UInt_t kCounterBackward  = 1u << 5;  // eventCounter decreased
   static constexpr UInt_t kTimeBackward     = 1u << 6;  // triggerTimeNs decreased
   static constexpr int kNQualityBits = 7;

   // Short description of a qualityFlags bit, e.g. "missing channel".
   static const char* GetQualityBitName(int bit);

   Event() = default;

   // Branch addresses point into this object, so it must not be copied.
   Event(const Event&) = delete;
   Event& operator=(const Event&) = delete;

   void Clear();

   void SetEventCounter(UInt_t eventCounter) { fEventCounter = eventCounter; }
   void SetTriggerTimeNs(ULong64_t timeNs) { fTriggerTimeNs = timeNs; }
   void SetQualityFlags(UInt_t flags) { fQualityFlags = flags; }

   // Starts a new channel block; following AddSample() calls append to it.
   void AddChannel(UChar_t channel);
   void AddSample(Short_t valueMv);

   UInt_t GetEventCounter() const { return fEventCounter; }
   ULong64_t GetTriggerTimeNs() const { return fTriggerTimeNs; }
   UInt_t GetNChannels() const { return fNChannels; }
   const std::vector<UChar_t>& GetChannelIds() const { return fChannelId; }
   const std::vector<UInt_t>& GetChannelNSamples() const { return fChannelNSamples; }
   const std::vector<Short_t>& GetSampleValuesMv() const { return fSampleValueMv; }
   UInt_t GetQualityFlags() const { return fQualityFlags; }

   // Channel block k in stored order (k < GetChannelIds().size(), else throws).
   Channel GetChannelAt(UInt_t k) const;

   // First block with this digitizer channel number; empty if it is absent.
   std::optional<Channel> GetChannel(int channelId) const;

   // All channel blocks in stored order.
   std::vector<Channel> GetChannels() const;

   // Creates the output branches of the events tree.
   void CreateBranches(TTree* tree);

   // Connects this object to an existing events tree for reading.
   void SetBranchAddresses(TTree* tree);

private:
   UInt_t fEventCounter = 0;
   ULong64_t fTriggerTimeNs = 0;
   UInt_t fNChannels = 0;
   std::vector<UChar_t> fChannelId;
   std::vector<UInt_t> fChannelNSamples;
   std::vector<Short_t> fSampleValueMv;
   UInt_t fQualityFlags = 0;

   // TTree::SetBranchAddress needs a pointer to a pointer for vectors.
   std::vector<UChar_t>* fChannelIdAddress = &fChannelId;
   std::vector<UInt_t>* fChannelNSamplesAddress = &fChannelNSamples;
   std::vector<Short_t>* fSampleValueMvAddress = &fSampleValueMv;
};

#endif
