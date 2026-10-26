# ENRGToolFlow Coding Standards

This document defines the common rules for all Tools, ToolChains and `shared/` code in ENRGToolFlow. The rules follow the style of the ToolFlow examples and the ROOT naming conventions. If a rule changes, this file is updated first.

> Items marked **[DECISION]** are proposals that are not final yet. The mark is removed once they are agreed on.

---

## 1. General

- Language: C++17, ROOT 6. Build system: CMake.
- **Everything in the repository is written in English:** identifiers, comments, log/error messages, config comments, README files and commit messages.
- Indentation: **3 spaces**, no tab characters.
- Maximum line length: 90 characters.
- Braces: the opening brace `{` always goes on its own line, for functions, classes, `if`/`else`, `for`, `while` and `switch` alike. `else` also starts on its own line. Always use braces, even for single-line blocks.

```cpp
void ComputeBaseline(const std::vector<double>& samples)
{
   if (samples.empty())
   {
      return;
   }
   else
   {
      ...
   }

   for (size_t i = 0; i < samples.size(); ++i)
   {
      ...
   }
}
```

## 2. Naming

| Item | Rule | Example |
|---|---|---|
| Tool / ToolChain directory | camelCase, verb + noun | `computeBaseline`, `drawEvents`, `muonSelection` |
| Class, struct | PascalCase | `WaveformReader`, `PulseFinder` |
| Function, method | PascalCase (ROOT style) | `FindPulses()`, `GetBaseline()` |
| Local variable, parameter | camelCase | `nSamples`, `eventCounter` |
| Class data member | `f` + PascalCase (ROOT style) | `fTree`, `fBaselineWindow` |
| Constant | `k` + PascalCase | `kAdcLsbMv`, `kNumChannels` |
| File | same as the class | `PulseFinder.h`, `PulseFinder.cpp` |
| Branch (written by us) | camelCase | `baselineMv`, `pulseChargePc` |

- Names of physical quantities **end with their unit**: `timeNs`, `amplitudeMv`, `chargePc`, `rateHz`. Branch names coming from ENRGDAQ (`value_mv`, `real_ns_timestamp`, ...) are kept as they are.
- Counts use the `n` prefix: `nEvents`, `nChannels`.
- Vector branches are named after the object they belong to, not after their type
  (no `Vec` suffix). Per-object vectors share the object name as prefix and have
  the length given by the matching `n` counter; event-level branches have no prefix.
  Example (`events` tree of convertWaveforms):

| Group | Prefix | Length | Branches |
|---|---|---|---|
| event | none | 1 | `eventCounter`, `triggerTimeNs`, `nChannels`, `qualityFlags` |
| channel | `channel` | `nChannels` | `channelId`, `channelNSamples` |
| sample | `sample` | Σ `channelNSamples` | `sampleValueMv` |

  A later pulse collection would follow the same rule: `nPulses`, `pulseChannelId`,
  `pulseTimeNs`, `pulseChargePc`.
- **[DECISION]** Existing `shared/` sources use `.cc`, Tools use `.cpp`. Proposal: new files use `.cpp` / `.h`, and the `shared/` CMake glob is updated to accept both extensions.

## 3. Tool structure

Every Tool is created with `./createTool.sh <toolName>`, never by hand.

```
tools/<toolName>/
├── main.cpp        # main() + <toolName>(iniReader)
├── config.ini      # all settings
├── README.md       # template in section 6
├── CMakeLists.txt
├── include/        # (optional) Tool-specific classes
└── src/
```

- `main()` only checks the arguments and the config, then calls the Tool function. No analysis code in `main()`.
- The Tool function **returns `int`**: 0 on success, non-zero on failure, and `main()` returns that value. This lets a ToolChain stop at the failing step. (The `void` return type in the ToolFlow template is changed accordingly.)
- A Tool does **one job**. "Compute the baseline" and "find pulses" are two Tools.
- Code used by two or more Tools moves to `shared/`; it is not copied.
- Shared code that keeps no state is written as free functions in a namespace, not
  as a class. Start with a single namespace and split it by topic only when it grows
  too large.
- Tools that open a graphics window (`TApplication`) are for visualisation only and are not used in ToolChains. Tools in a ToolChain run in batch mode.

## 4. Configuration (`config.ini`)

- **Every** adjustable value (file paths, windows, thresholds, channel lists) is read from the config. No magic numbers in the code. Physical constants are the exception; they are defined in `shared/` as `k...` constants.
- Standard sections:

```ini
[input]
rootFile = ../runs/run_103_waveforms.root   ; relative to ~/Desktop/enrg/build
treeName = digitizer_waveforms

[output]
rootFile = ../output/run_103_baseline.root

[analysis]
baselineWindowNs = 100      ; number of leading samples used for the baseline
channels = 0,1,2,3,4,5,6,7  ; digitizer channels to process
maxEvents = -1              ; -1 = all events
```

- Keys are camelCase and include the unit.
- Every key has a short comment (after `;`).
- Every key read in the code must be present in `config.ini`. If a required key is missing, the Tool stops with an error instead of silently using a default.
- Every Tool has a `maxEvents` key, so quick tests can run on a small subset.

## 5. Input and output

- **Input files are never overwritten.** Output always goes to a new file.
- A Tool does not have to write a ROOT file; it may, for example, only read one and
  produce histograms.
- Output file names show the run number and the processing step: `run_<N>_<step>.root`.
- When reading large TTrees, enable only the branches that are needed:

```cpp
tree->SetBranchStatus("*", 0);
tree->SetBranchStatus("value_mv", 1);
```

- **[DECISION]** Every output ROOT file stores provenance as `TNamed` objects: the Tool name, the full config file content and the git commit hash (`git rev-parse --short HEAD`). A common helper in `shared/` does this.

### 5.1 Passing results between Tools

- A Tool in a ToolChain passes its per-event results to the next Tools as a friend
  tree: its output holds only its new branches plus `eventCounter`, and the next
  Tools join it to their input with `TTree::AddFriend`.
- The output has exactly one entry per input entry, in the same order. No event is
  skipped; an event that cannot be processed still gets an entry, marked with a flag.
- A Tool that reads friend files checks that each one has the same number of entries
  as its input tree and the same `eventCounter` in every entry, and stops with an
  error otherwise.

## 6. README template (one per Tool)

```markdown
# <toolName>

## Purpose
One or two sentences.

## Input
File type, TTree and branch names.

## Output
File, TTree, new branches with units, histograms.

## Configuration
| Section | Key | Unit | Description |
|---|---|---|---|

## Usage
./bin/<toolName> configs/<toolName>/config.ini

## Notes / known limitations
```

## 7. ToolChains

- ToolChains are created with `./createToolChain.sh <chainName> <tool1> <tool2> ...`.
- The ToolChain name describes the workflow: `muonCharge`, `ambeNeutronTag`.
- The `.ini` file of every Tool in the chain lives in the ToolChain directory. The output path of one Tool must match the input path of the next.

## 8. Errors and messages

- Errors go to `std::cerr` with the prefix `Error: `, warnings with `Warning: `.
- If a file cannot be opened (`IsZombie()`) or a TTree is missing, the Tool returns a non-zero code and stops.
- At the end, every Tool prints a summary: input file, output file, number of processed and skipped events.
- In long loops, progress is printed only at intervals (e.g. every 10%).

## 9. Git

- `main` always compiles. New Tools are developed on a separate branch and merged into `main` when finished.
- One commit = one logical change. Messages are short, in English, in the imperative: `computeBaseline: add per-channel RMS output`.
- `.root`, `.pdf`, `.png` files and `build/` are never committed.
- Before committing: the code compiles, the Tool runs on a small subset (`maxEvents`), and the README is up to date.
