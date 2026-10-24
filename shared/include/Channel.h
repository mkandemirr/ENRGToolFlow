#ifndef CHANNEL_H
#define CHANNEL_H

#include "Waveform.h"

// Read-only view of one channel block of an Event.
//
// Per-channel results of later Tools (baseline, pulses) are meant to be added
// here. Like Waveform, a Channel is valid only until its Event is modified or
// the next tree entry is read.
class Channel
{
public:
   Channel(int channelId, const Waveform& waveform)
      : fChannelId(channelId), fWaveform(waveform)
   {
   }

   // Digitizer channel number (returned as int so that it prints as a number).
   int GetChannelId() const { return fChannelId; }

   const Waveform& GetWaveform() const { return fWaveform; }

private:
   int fChannelId;
   Waveform fWaveform;
};

#endif
