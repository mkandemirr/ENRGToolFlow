#ifndef RAW_WAVEFORM_READER_H
#define RAW_WAVEFORM_READER_H

#include <string>

#include <Rtypes.h>

class TFile;
class TTree;
class Event;

// Reads a raw ENRGDAQ file (one row per sample) and groups the rows into events.
//
// A new event starts when event_counter changes. Inside an event, a new channel
// block starts when channel changes or when sample_index restarts at 0. The
// record length is never assumed; it is found by counting rows.
//
// The reader stops with an error if the rows cannot be stored losslessly as
// (triggerTimeNs, channel blocks starting at sample_index 0):
//   - a block does not start at sample_index 0,
//   - sample_index does not increase by exactly 1 inside a block,
//   - real_ns_timestamp != triggerTimeNs + sample_index.
class RawWaveformReader
{
public:
   enum class Status
   {
      kEvent,      // an event was read
      kEndOfTree,  // no rows left
      kError       // inconsistent data, message printed to std::cerr
   };

   RawWaveformReader() = default;
   ~RawWaveformReader();

   RawWaveformReader(const RawWaveformReader&) = delete;
   RawWaveformReader& operator=(const RawWaveformReader&) = delete;

   // Returns false (and prints an error) if the file or tree cannot be used.
   bool Open(const std::string& fileName, const std::string& treeName,
             Long64_t cacheSizeBytes);

   Status ReadNextEvent(Event& event);

   Long64_t GetNEntries() const { return fNEntries; }

   // Number of raw rows consumed so far, including a row already read ahead.
   Long64_t GetNEntriesRead() const { return fNextEntry; }

private:
   void LoadRow(Long64_t entry);
   void PrintRowError(const std::string& message) const;

   TFile* fFile = nullptr;
   TTree* fTree = nullptr;
   Long64_t fNEntries = 0;
   Long64_t fNextEntry = 0;
   bool fHasPendingRow = false;  // the loaded row belongs to the next event

   // Branch buffers of the current row.
   ULong64_t fTimestampNs = 0;
   UInt_t fEventCounter = 0;
   UChar_t fChannel = 0;
   UShort_t fSampleIndex16 = 0;  // sample_index is uint16 before 15.09.2026
   UInt_t fSampleIndex32 = 0;    // and uint32 afterwards
   bool fSampleIndexIs32 = false;
   UInt_t fSampleIndex = 0;
   Short_t fValueMv = 0;
};

#endif
