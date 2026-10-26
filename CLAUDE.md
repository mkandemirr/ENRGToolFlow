# ENRGToolFlow: analysis code for the ENRG Cherenkov detector

This repository contains the analysis code for the ~1 t Gd-doped water Cherenkov detector in the ENRG lab. It is derived from the [ToolFlow](https://github.com/mkandemirr/ToolFlow) skeleton.

- Talk to the user in Turkish, with the English term in parentheses for important terms. Everything written into the repository (code, comments, READMEs, commit messages) is in English.
- Mark anything that has not been verified as "to be confirmed". Never present a guess as a fact.

## Directory layout (including outside the repo)

```
~/Desktop/enrg/
├── docs/          # notes (docx), Dedektör Ar-Ge.pdf, runs_history.xlsx (not in git)
├── runs/          # raw data: run_<N>_waveforms.root (not in git, files are several GB)
├── build/         # out-of-source build directory (outside the repository)
├── output/        # ROOT/PDF output of the Tools (not in git)
└── ENRGToolFlow/  # this repository
```

- Detailed physics and electronics notes are in `../docs/` (in Turkish). Check them first when unsure.
- `.root`, `.pdf`, `.png` files and the `build/` and `bin/` directories are git-ignored. Never commit data files.

## ToolFlow structure

- `tools/<Tool>/`: each Tool is a separate executable with `main.cpp`, `config.ini`, `CMakeLists.txt` and `README.md`. Create new Tools with `./createTool.sh <name>`; the script also updates `tools/toolRegistry.txt`.
- `toolChains/<Chain>/`: a `.tchain` file that runs Tools in sequence, plus one `.ini` per Tool. Create with `./createToolChain.sh <Chain> <Tool1> <Tool2> ...`.
- `shared/`: classes used by more than one Tool.
- `iniReader/`: INI parser (inih).
- Tools communicate through ROOT files and never overwrite the input file. Within a ToolChain, results are passed on as friend trees (`CODING_STANDARDS.md` §5.1).
- The ToolFlow example Tools were removed; `tools/convertWaveforms` is the reference Tool.

Build and run (out-of-source, in `~/Desktop/enrg/build`; never create a build directory inside the repository):

```bash
mkdir -p ~/Desktop/enrg/build && cd ~/Desktop/enrg/build
cmake ../ENRGToolFlow [-DTOOLS="tools/X;tools/Y"] [-DTOOLCHAINS="toolChains/Z"]
cmake --build . -j
./bin/<tool> configs/<tool>/config.ini
./bin/runPipeline toolChains/<Chain>/<Chain>.tchain
```

Tools are run from `~/Desktop/enrg/build`, so relative paths in config files are resolved from there (e.g. `../runs/run_103_waveforms.root`).

## Detector (summary)

- Tank: polyethylene, Ø105 cm × 120 cm, 1000 L, with 1 kg Gd₂(SO₄)₃·8H₂O (~0.042% Gd).
- 8 × Hamamatsu R14688-100 (8", QE ~35%, rise time 2.2 ns, TTS ~1 ns), single-cable positive-HV bases. HV and therefore gain can change from run to run.
- The trigger configuration (trigger source, thresholds, etc.) changes from run to run. Never assume a specific trigger in the code; take it from the run information (`../docs/runs_history.xlsx`) or from the config.
- Electronics chain: SY4527 HV → pickoff/splitter → 2× N625 fan-out. One branch goes to the VX1751 digitizer; the other goes to N979 → N841 discriminator → N1081B logic/counters. The VX1751 is read out by ENRGDAQ over an optical link (A3818).
- **To be confirmed:** channel → PMT mapping, position of the N979 in the chain, cable lengths.

## Digitizer: CAEN VX1751

- 10 bit, 1 GS/s (1 sample = 1 ns), 1 Vpp input, 50 Ω.
- 1 LSB = 1 V / 1024 ≈ 0.977 mV.
- Per-channel DC offset sets where the baseline sits inside the 1 V window, and therefore where pulses clip. It can change from run to run, so the code must measure the baseline from the data in every event and must not assume a clipping level.
- Trigger clock 125 MHz (8 ns). Trigger Time Tag (TTT) resolution 16 ns.
- Approximate charge: Q[pC] ≈ Σ|value_mv| × 1 ns / 50 Ω. 1 pe = gain × 1.6e-19 C (e.g. 1.6 pC at gain 1e7); the gain must come from calibration, not be hard-coded.

## Data format (ENRGDAQ, `run_<N>_waveforms.root`)

TTree `digitizer_waveforms`. **Each row is one sample**, not one event.

| Branch | Type | Meaning |
|---|---|---|
| `real_ns_timestamp` | uint64 | t₀ + sample_index [ns]; t₀ = TTT × 8 ns + software rollover correction; reference is the start of the run |
| `event_counter` | uint32 | hardware event counter; gaps are possible |
| `channel` | uint8 | digitizer channel (0–7) |
| `sample_index` | uint16 (uint32 after 15.09.2026) | sample number within the record [ns] |
| `value_mv` | int16 | (ADC − P_DAQ) × 1000/1024, truncated toward zero |

- Row order: event → channel → sample. Without filtering, 1 event = C × N rows (C channels, N record length).
- P_DAQ is computed per event and per channel as the floor of the mean of the first 10% of the samples, so `value_mv` is already baseline-subtracted. Because of the truncation, ±1 ADC count becomes 0 mV.
- ENRGDAQ writes at most N_max samples per channel and drops the rest: 2048 before 06.06.2026, 65 536 from 06.06 to 15.09.2026, 131 072 afterwards.
- If `filter_threshold_mv` is enabled, samples below the threshold are not written (zero suppression), and N can no longer be derived from the row count. Group events by changes in `event_counter` / `channel`.
- Never assume a fixed record length; count the rows for each event/channel.

## Coding rules

Before writing a Tool, a ToolChain or a `shared/` class, read **`CODING_STANDARDS.md`** and follow it.

**Changes to `shared/` always need the user's approval.** `shared/` is the code that all Tools have in common, and the user wants to know exactly what is in it. Before writing a Tool, check whether it needs a new `shared/` class or a change to an existing one (new methods, changed interfaces). If it does, stop and ask the user first: say which class, why it belongs in `shared/` rather than in the Tool, and what its interface would look like. Do not create or change anything in `shared/` until the user has approved it. This also applies to code moved out of an existing Tool into `shared/`.

**Agree on the config before writing a Tool.** Before any code of a new Tool is written, propose its complete `config.ini` to the user (every section and key, with unit, default value and comment, and what each key controls) and ask the user to approve it. List the open questions about the parameters explicitly. Do not run `./createTool.sh` or write `main.cpp` until the user has approved the config. Adding, removing or renaming a key in an existing Tool also needs the user's approval first.
