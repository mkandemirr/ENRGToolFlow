# convertWaveforms

## Purpose
Converts a raw ENRGDAQ file (one row per sample) into a TTree with one entry per
event. The conversion is lossless: every raw row can be rebuilt from the output.

## Input
Raw ENRGDAQ file `run_<N>_waveforms.root`, TTree `digitizer_waveforms`, branches
`real_ns_timestamp`, `event_counter`, `channel`, `sample_index` (uint16 or uint32,
detected automatically) and `value_mv`. The file is read through
`shared/RawWaveformReader`.

## Output
File `run_<N>_events.root` with a single TTree `events` (one entry per event):

| Branch | Type | Length | Description |
|---|---|---|---|
| `eventCounter` | `UInt_t` | 1 | raw `event_counter` |
| `triggerTimeNs` | `ULong64_t` | 1 | t0 = `real_ns_timestamp − sample_index` [ns since run start] |
| `nChannels` | `UInt_t` | 1 | number of channel blocks (= `channelId.size()`) |
| `channelId` | `vector<UChar_t>` | nChannels | digitizer channel of each block, in raw order |
| `channelNSamples` | `vector<UInt_t>` | nChannels | number of samples in each block |
| `sampleValueMv` | `vector<Short_t>` | Σ channelNSamples | raw `value_mv` of all blocks, one block after another [mV] |
| `qualityFlags` | `UInt_t` | 1 | informational bit mask, see below |

Branch names follow the grouping rule of CODING_STANDARDS.md: event-level
branches have no prefix, per-channel vectors start with `channel` (length
`nChannels`) and per-sample vectors start with `sample` (length
Σ `channelNSamples`).

Sample `i` of block `k` is `sampleValueMv[offset + i]`, where `offset` is the sum of
`channelNSamples[0..k-1]`. It was recorded at `triggerTimeNs + i` and had
`sample_index = i`.

To read the tree, use `shared/Event`, which returns `Channel` and `Waveform` views
(no copies; valid until the next `GetEntry`):

```cpp
Event event;
event.SetBranchAddresses(tree);
tree->GetEntry(entry);

if (auto channel = event.GetChannel(3))   // empty if channel 3 is absent
{
   const Waveform& waveform = channel->GetWaveform();
   Short_t valueMv = waveform.GetValueMv(100);
   ULong64_t timeNs = waveform.GetTimeNs(100);   // triggerTimeNs + 100
}

for (const Channel& channel : event.GetChannels())   // all blocks, stored order
{
   ...
}
```

`qualityFlags` bits (a flagged event is still complete and is never skipped):

| Bit | Value | Meaning |
|---|---|---|
| 0 | 1 | a channel from `expectedChannels` is missing |
| 1 | 2 | a channel appears in more than one block |
| 2 | 4 | channel blocks have different lengths |
| 3 | 8 | a block length equals `daqMaxSamplesPerChannel` (record may be truncated by ENRGDAQ) |
| 4 | 16 | `eventCounter` increased by more than 1 since the previous event |
| 5 | 32 | `eventCounter` decreased |
| 6 | 64 | `triggerTimeNs` decreased |

## Configuration
| Section | Key | Unit | Description |
|---|---|---|---|
| input | `rootFile` | | raw ENRGDAQ file |
| input | `treeName` | | raw TTree name (`digitizer_waveforms`) |
| input | `readCacheSizeMb` | MB | TTreeCache size for reading; affects speed only |
| output | `rootFile` | | output file; must differ from the input file |
| output | `treeName` | | output TTree name (`events`) |
| output | `compressionSetting` | | ROOT compression, 100 × algorithm + level (505 = ZSTD level 5) |
| output | `overwriteOutput` | | `false` = stop if the output file already exists |
| analysis | `expectedChannels` | | comma-separated channel list, used only for bit 0 |
| analysis | `daqMaxSamplesPerChannel` | samples | ENRGDAQ per-channel limit of the run, used only for bit 3: 2048 before 06.06.2026, 65536 from 06.06 to 15.09.2026, 131072 afterwards |
| analysis | `maxEvents` | events | number of events to convert, -1 = all |

All keys are required.

## Usage
```bash
./bin/convertWaveforms configs/convertWaveforms/config.ini
```

## Notes / known limitations
- The Tool stops with an error (non-zero exit code) if the raw rows cannot be
  stored losslessly: a channel block that does not start at `sample_index` 0,
  `sample_index` not increasing by exactly 1 (e.g. zero suppression), or
  `real_ns_timestamp != triggerTimeNs + sample_index`. Zero-suppressed data is
  therefore not supported yet.
- Two events with the same `event_counter` in adjacent rows cannot be separated;
  the timestamp check then stops the Tool.
- Only the `events` tree is written. The summary (flag counts, record length
  distribution, missing events) is printed to the terminal.
