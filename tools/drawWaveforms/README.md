# drawWaveforms

## Purpose
Draws the raw waveforms of selected events into a PDF, one page per event and one
row per channel: the full record window on the left and a fixed zoom window on the
right. The Tool only displays the data; it computes no baseline, finds no pulses and
flags no saturation. Those are the jobs of the analysis Tools.

## Input
Events file `run_<N>_events.root` written by convertWaveforms, TTree `events`
(branches `eventCounter`, `triggerTimeNs`, `channelId`, `channelNSamples`,
`sampleValueMv`, `qualityFlags`), read through `shared/Event`.

## Output
PDF file `run_<N>_drawWaveforms.pdf`. Each page shows:

- header: input file, tree entry, `eventCounter` (the two differ after counter
  gaps), t0 (`triggerTimeNs`) and the `qualityFlags` of the event;
- left panel per channel: the full window in µs, drawn as a min/max envelope in
  `nEnvelopeBins` bins (single-sample spikes stay visible); the two dashed red lines
  mark the zoom window; the text gives the minimum, its position and the maximum
  of all samples of the channel;
- right panel per channel: every sample from `zoomStartNs` to `zoomEndNs`.

The values are `value_mv` as written by ENRGDAQ, which is already
baseline-subtracted per event and channel, so 0 mV is the reference on the y axis.

### Zoom window

The zoom window is the same for every event and every channel. Where the prompt
pulse falls in the record depends on the digitizer settings of the run (record
length and post-trigger size), so the window must be set per run. In run 103 the
prompt pulses peak at sample index ≈ 11 206–11 224
(`docs/ENRGDAQ_Veri_Formati.docx`, section 5.2), hence the default 11150–11350.

If the window extends past the end of a channel's record, the panel is cut at the
last sample. If it starts after the last sample, the panel says so instead.

**Units:** `zoomStartNs` and `zoomEndNs` are in ns (1 sample = 1 ns), while the
time axis of the left-hand panels is in µs. A pulse seen at 11.2 µs on the left is
at sample 11200, so the window must be written as 11150–11350, not 11.15–11.35.

### Full-window envelope (`nEnvelopeBins`)

The left-hand panel shows the whole record of a channel (65 536 samples in run
103), but its frame is only about 700 pixels wide (the canvas is 1600 pixels wide
and holds two columns). Drawing every sample would put about 90 points on each pixel
column, make the PDF large and slow down drawing, without showing anything more.
The panel is therefore drawn as a min/max envelope:

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
samples, so channels of different length are handled correctly. The minimum and
maximum written in the panel come from all samples, not from the envelope.

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
| analysis | `zoomStartNs` | ns | first sample of the zoom panel; must be ≥ 0 |
| analysis | `zoomEndNs` | ns | last sample of the zoom panel; must be ≥ `zoomStartNs` |
| analysis | `nEnvelopeBins` | | bins of the full-window envelope |
| analysis | `minYHalfRangeMv` | mV | y axis spans at least 0 ± this value |
| analysis | `yPaddingMv` | mV | extra y range beyond the waveform minimum and maximum |

All keys are required.

## Usage
```bash
./bin/drawWaveforms configs/drawWaveforms/config.ini
```

## Notes / known limitations
- Visualisation only; runs in batch mode and writes a PDF (no interactive window).
- Results of analysis Tools (baseline, pulses, saturation) are not shown yet. They
  can later be drawn on top of the waveforms by reading their friend trees.
