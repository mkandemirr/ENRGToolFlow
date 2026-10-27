# findTriggerPosition

## Purpose
Measures where the prompt (triggering) pulse sits in the record of each channel, so
that later Tools can place their baseline and pulse windows. It runs once per run,
outside the eventBuilder ToolChain.

## Input
Events file `run_<N>_events.root` written by convertWaveforms, TTree `events`
(branches `channelId`, `channelNSamples`, `sampleValueMv`), read through
`shared/Event`. All events are used, also those with non-zero `qualityFlags`; a
channel missing from an event is skipped for that event.

## Output
PDF file `run_<N>_triggerPosition.pdf` and a summary in the terminal. No ROOT file
is written; the measured position is copied by hand into the config of later Tools.

- Page 1: summary table (same as the terminal).
- One page per channel:
  - top left: average waveform over the full record;
  - top right: occupancy over the full record (log scale);
  - bottom left / right: the same two histograms around the measured position
    (± `plotHalfWindowNs`).

## Method
Both histograms have one bin per sample (1 sample = 1 ns). Records of different
length are handled sample by sample; no fixed record length is assumed.

**Average waveform.** For every sample index i, the mean of `value_mv` over all
events whose record contains i. Random dark pulses average out; the prompt pulse is
at the same place in every event and stays as a dip.
- `T_min`: index of the minimum of the average waveform.
- `T_cf` (leading edge): walking back from `T_min`, the first point where the
  average waveform crosses `cfFraction` × minimum, linearly interpolated between
  the two neighbouring samples. This gives a sub-ns position that does not depend
  on the pulse height.

**Occupancy.** For every event, the index of its lowest sample (the first one if
several share the value) is filled into a histogram. Events with a prompt pulse in
this channel pile up in a sharp peak; events without one spread over the record as
a flat floor. One event adds one count, so a single large event cannot pull the
result.
- `T_occ`: index of the highest bin.
- `FWHM`: full width at half maximum of that peak, linearly interpolated; a
  measure of the event-to-event spread (jitter) of the pulse position.

**Run results.**
- Trigger position of the run: median of `T_cf` over the channels.
- `dT`: `T_cf` of each channel minus that median (cable and transit-time offsets).
- Nominal position, if `postTriggerPercent` is given: `N × (1 − p/100)`, with `N`
  the most frequent record length. The difference to the measured position is
  printed; a constant offset from run to run is the delay of the trigger chain.
- ENRGDAQ baseline check: ENRGDAQ subtracts the mean of the first
  `daqBaselineFraction × N` samples from `value_mv`. A warning is printed if a
  channel's `T_cf` lies inside that window, because the DAQ baseline would then
  include the prompt pulse.

## Configuration
| Section | Key | Unit | Description |
|---|---|---|---|
| input | `rootFile` | | events file from convertWaveforms |
| input | `treeName` | | events TTree name (`events`) |
| output | `pdfFile` | | output PDF |
| output | `overwriteOutput` | | `false` = stop if the output file already exists |
| analysis | `maxEvents` | events | number of events to use, -1 = all |
| analysis | `channels` | | channels to analyse |
| analysis | `postTriggerPercent` | % | digitizer post-trigger setting; -1 = unknown, comparison skipped |
| analysis | `cfFraction` | | fraction of the average-waveform minimum that defines the leading edge (0–1) |
| analysis | `daqBaselineFraction` | | fraction of the record ENRGDAQ uses for its baseline |
| analysis | `plotHalfWindowNs` | ns | half width of the zoomed plots (display only) |

All keys are required.

## Usage
```bash
./bin/findTriggerPosition configs/findTriggerPosition/config.ini
```

## Notes / known limitations
- The trigger position is assumed constant during the run. A drift would only
  show as a wider occupancy peak; a position-versus-time histogram is not made yet.
- The fraction of events in which a channel sees the prompt pulse is not computed.
- Run 103 (12 442 events, all channels): trigger position 11 208.6 ns, channel
  offsets within ±2.1 ns, occupancy FWHM 15–17 ns; about 35 s for the full run.
