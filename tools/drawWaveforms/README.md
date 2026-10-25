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

### Zoom centre (dashed red line)

The dashed red line marks the zoom centre: the sample on which the right-hand zoom
panels are centred. It is a time position (a sample index), not a voltage, and it is
the same for all channels of the page. It is found as follows:

1. Only samples inside [`pulseSearchStartNs`, `pulseSearchEndNs`] are considered.
   A deeper pulse outside this window is ignored.
2. For every drawn channel, the depth of each of these samples below the channel's
   displayed baseline (the mean of its first `baselineWindowNs` samples) is
   computed: `value − baseline`.
3. The sample with the most negative depth over all drawn channels becomes the zoom
   centre, provided it is deeper than `pulseThresholdMv`.
4. If no sample is deeper than `pulseThresholdMv` (no clear pulse in the window),
   the middle of the window is used instead.

The line therefore marks the deepest pulse of the event, not the minimum of each
channel. The minimum of each channel and its position are written in the panel
("min … mV @ … ns"); that value is taken over the whole record, not only the search
window. Examples from run 103:

- Event 2: the zoom centre is 11218, the position of the −37 mV minimum of channel
  3 (and of channel 6). The minimum of channel 0 is at 11224, so in channel 0 the
  line is 6 ns away from that channel's own minimum.
- Event 4: no sample in the window is deeper than 8 mV, so the zoom centre is the
  middle of the window, (10000 + 12500) / 2 = 11250.

One common line is used because all channels of an event are recorded with the same
trigger; it makes the relative timing of the pulses in the different channels easy
to compare.

The trigger time is not stored in the file. Where the prompt pulse falls in the
record depends on the digitizer settings of the run (record length and
post-trigger size), so the search window must be set per run. In run 103 the prompt
pulses peak at sample index ≈ 11 206–11 224 (`docs/ENRGDAQ_Veri_Formati.docx`,
section 5.2), hence the default window 10000–12500. The same reasoning sets
`baselineWindowNs = 5000`: the first 5000 samples lie well before the trigger.

### Zoom range and page header

The right-hand panels show the samples within ± `zoomHalfWidthNs` of the zoom
centre:

    right-hand range = [zoom centre − zoomHalfWidthNs, zoom centre + zoomHalfWidthNs]

The search window [`pulseSearchStartNs`, `pulseSearchEndNs`] only decides where the
zoom centre is looked for; it is not the range that is drawn. With
`zoomHalfWidthNs = 100` every zoom panel is 200 ns wide, and the zoom centre is
always in the middle of it. The dashed line is drawn only in the left-hand panels.

The second line of the page header gives both values for that event, for example:

    Right: samples 11107-11307. Dashed line: zoom centre (11207).

Run 103, search window 10000–12500, `zoomHalfWidthNs = 100`:

| Entry | Zoom centre | Right-hand range |
|---|---|---|
| 2 | 11218 (channels 3 and 6, −37 mV) | 11118–11318 |
| 3 | 11208 | 11108–11308 |
| 4 | 11250 (no pulse; middle of the window) | 11150–11350 |
| 5 | 11212 | 11112–11312 |

**Units:** all `...Ns` keys are in ns (1 sample = 1 ns), while the time axis of the
left-hand panels is in µs. A prompt pulse seen at 11.2 µs on the left is at sample
11200, so the search window must be written as 10000–12500, not 10–12.5 or 100–125.
With `pulseSearchStartNs = 100` and `pulseSearchEndNs = 125`, for example, no pulse
is found in the window, the zoom centre falls back to 112 and the right-hand panels
show samples 12–212 instead of the pulse.

### Full-window envelope (`nEnvelopeBins`)

The left-hand panel shows the whole record of a channel (65 536 samples in run
103), but its frame is only about 700 pixels wide (the canvas is 1600 pixels wide
and holds two columns). Drawing every sample would put about 90 points on each pixel
column, make the PDF large and slow down drawing, without showing anything more. The panel is therefore drawn as a min/max envelope:

1. The samples of the channel are split into `nEnvelopeBins` equal bins. With
   65 536 samples and `nEnvelopeBins = 2000`, each bin holds about 33 samples, i.e.
   covers about 33 ns.
2. For each bin, the minimum and the maximum sample value are found.
3. Each bin is drawn as a vertical line from its minimum to its maximum, at the
   centre of the bin.

The panel then has `2 × nEnvelopeBins` points (4000) instead of 65 536. Because the
minimum and maximum of every bin are kept, pulses and single-sample spikes keep
their full height: a one-sample dip to −37 mV is the minimum of its bin and is drawn
at −37 mV. Only the shape inside a bin is lost, and that is shown in the right-hand
zoom panel, where every sample is drawn.

| `nEnvelopeBins` | Samples per bin (65 536 samples) | Effect |
|---|---|---|
| 500 | ≈ 131 | coarser full-window view, smaller PDF |
| 2000 (default) | ≈ 33 | about three bins per pixel column |
| 10 000 | ≈ 7 | finer full-window view, larger PDF |
| ≥ number of samples | 1 | every sample drawn (capped at the number of samples) |

The bin boundaries are computed per channel from that channel's own number of
samples, so channels of different length are handled correctly. The minimum,
baseline and rms written in the panel are computed from all samples, not from the
envelope.

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
