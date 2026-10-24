# drawWaveforms

## Purpose
Draws the waveforms of selected events into a PDF, one page per event and one row
per channel: the full record window on the left and a zoom around the main pulse on
the right.

## Input
Events file `run_<N>_events.root` written by convertWaveforms, TTree `events`
(branches `eventCounter`, `triggerTimeNs`, `channelId`, `channelNSamples`,
`sampleValueMv`, `qualityFlags`), read through `shared/Event`.

## Output
PDF file `run_<N>_drawWaveforms.pdf`. Each page shows:

- header: input file, tree entry, `eventCounter` (the two differ after counter
  gaps), t0 (`triggerTimeNs`) and the `qualityFlags` of the event;
- left panel per channel: the full window in µs, drawn as a min/max envelope in
  `nEnvelopeBins` bins (single-sample spikes stay visible); the dashed red line
  marks the zoom centre; the text gives the minimum and its position, and the mean
  and rms of the first `baselineWindowNs` samples;
- right panel per channel: every sample within ±`zoomHalfWidthNs` of the zoom
  centre;
- a red "possible saturation" label when the minimum is deeper than
  `minSaturationDepthMv` below the baseline and at least `minPlateauSamples`
  consecutive samples sit at that minimum.

The zoom centre is the deepest sample below baseline in
[`pulseSearchStartNs`, `pulseSearchEndNs`] over all drawn channels, or the middle of
that window if no sample is deeper than `pulseThresholdMv`.

## Configuration
| Section | Key | Unit | Description |
|---|---|---|---|
| input | `rootFile` | | events file from convertWaveforms |
| input | `treeName` | | events TTree name (`events`) |
| output | `pdfFile` | | output PDF |
| output | `overwriteOutput` | | `false` = stop if the output file already exists |
| analysis | `firstEntry` | entry | first tree entry to draw |
| analysis | `maxEvents` | events | number of events to draw, -1 = all |
| analysis | `channels` | | channels to draw, one row each, in this order |
| analysis | `baselineWindowNs` | ns | leading samples used for the displayed baseline and rms |
| analysis | `pulseSearchStartNs` | ns | start of the zoom-centre search window |
| analysis | `pulseSearchEndNs` | ns | end of the zoom-centre search window |
| analysis | `pulseThresholdMv` | mV | minimum depth below baseline that counts as a pulse |
| analysis | `zoomHalfWidthNs` | ns | half width of the zoom panel |
| analysis | `nEnvelopeBins` | | bins of the full-window envelope |
| analysis | `minPlateauSamples` | samples | consecutive samples at the minimum for "possible saturation" |
| analysis | `minSaturationDepthMv` | mV | minimum depth below baseline for "possible saturation" |
| analysis | `minYHalfRangeMv` | mV | y axis spans at least baseline ± this value |
| analysis | `yPaddingMv` | mV | extra y range beyond the waveform minimum and maximum |

All keys are required.

## Usage
```bash
./bin/drawWaveforms configs/drawWaveforms/config.ini
```

## Notes / known limitations
- Visualisation only; runs in batch mode and writes a PDF (no interactive window).
- `value_mv` is already baseline-subtracted by ENRGDAQ; the displayed baseline is
  the residual offset of the first samples, not a calibrated baseline.
- The pulse search window depends on the trigger configuration of the run and must
  be set per run.
- The "possible saturation" criterion does not use the real clipping level, which
  depends on the DC offset and is not stored in the file. `minSaturationDepthMv`
  must be set per run, just above the clipping depth given by the DC offset; it
  also keeps small pulses (whose peak often has a few equal samples) from being
  flagged. The default of 900 mV is for run 103, where the baseline sits at about
  1005 of 1023 ADC counts and the lower limit is about -969 to -981 mV
  (`docs/Taban_Cizgisi_ve_Pedestal.docx`). Not yet tested on a saturated event.
- Events are selected by tree entry; the header also shows `eventCounter`.
