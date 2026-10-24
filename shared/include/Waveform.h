#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <Rtypes.h>

// Read-only view of the samples of one channel block of an Event.
//
// A Waveform does not own its samples; it points into the sampleValueMv vector
// of the Event it came from. It is valid only until that Event is modified or
// the next tree entry is read.
class Waveform
{
public:
   Waveform(const Short_t* valuesMv, UInt_t nSamples, ULong64_t triggerTimeNs)
      : fValuesMv(valuesMv), fNSamples(nSamples), fTriggerTimeNs(triggerTimeNs)
   {
   }

   UInt_t GetNSamples() const { return fNSamples; }

   // Sample value [mV]; index must be smaller than GetNSamples() (not checked).
   Short_t GetValueMv(UInt_t index) const { return fValuesMv[index]; }

   // Recording time of a sample [ns since run start]. One sample is 1 ns.
   ULong64_t GetTimeNs(UInt_t index) const { return fTriggerTimeNs + index; }

   // Pointer to the first sample, for loops and algorithms over all samples.
   const Short_t* GetValuesMv() const { return fValuesMv; }

private:
   const Short_t* fValuesMv;
   UInt_t fNSamples;
   ULong64_t fTriggerTimeNs;
};

#endif
