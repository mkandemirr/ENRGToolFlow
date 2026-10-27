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

`qualityFlags` is computed for every event: the sum of the values of the checks
that found a problem in that event; 0 means no problem. Bits 0–3 look at the event
alone, bits 4–6 compare it with the previous event (always 0 for the first event).
A flagged event is still complete and is never skipped. Each check can
be switched off in the config; its bit is then always 0.

| Bit | Value | Config switch | Meaning |
|---|---|---|---|
| 0 | 1 | `checkMissingChannel` | a channel from `expectedChannels` is missing |
| 1 | 2 | `checkDuplicateChannel` | a channel appears in more than one block |
| 2 | 4 | `checkUnequalLength` | channel blocks have different lengths |
| 3 | 8 | `checkDaqLimit` | a block length equals `daqMaxSamplesPerChannel` (record may be truncated by ENRGDAQ) |
| 4 | 16 | `checkCounterGap` | `eventCounter` increased by more than 1 since the previous event |
| 5 | 32 | `checkCounterBackward` | `eventCounter` decreased |
| 6 | 64 | `checkTimeBackward` | `triggerTimeNs` decreased |

Example: `qualityFlags = 9` = 8 + 1, i.e. a missing channel and a block at the DAQ
limit. In ROOT, select events with one problem with a bitwise AND, e.g.
`events->Draw("eventCounter", "(qualityFlags & 8) != 0")`.

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
| analysis | `maxEvents` | events | number of events to convert, -1 = all |
| qualityFlags | `check...` | | seven switches, `true` = check is done, `false` = its bit is always 0 (see the bit table above) |
| qualityFlags | `expectedChannels` | | comma-separated channel list, used only for bit 0 |
| qualityFlags | `daqMaxSamplesPerChannel` | samples | ENRGDAQ per-channel limit of the run, used only for bit 3: 2048 before 06.06.2026, 65536 from 06.06 to 15.09.2026, 131072 afterwards |

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
  distribution, missing events) is printed to the terminal. A check switched off
  in the config is shown there as "not checked"; the output file itself does not
  record which checks were on.
