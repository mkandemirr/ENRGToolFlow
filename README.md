# ENRGToolFlow

ToolFlow provides a lightweight C++ project skeleton for developing modular scientific analysis software using [CERN ROOT](https://root.cern/).

The main idea behind ToolFlow is to combine the simplicity of traditional [ROOT macros](https://root.cern/primer/#root-macros) with the modular **Tool** and **ToolChain** concepts used in [ToolAnalysis](https://github.com/ANNIEsoft/ToolAnalysis).

Instead of writing standalone ROOT macros, analyses are implemented as reusable **Tools** that can be executed individually or combined into **ToolChains**. Writing a Tool feels very similar to writing a ROOT macro, while providing the benefits of a standard C++ project such as CMake integration, configuration files( using [inih](https://github.com/jtilly/inih)), and the ability to grow into larger applications by adding additional source files, classes, and libraries.

---

# Features

- ROOT macro-like development
- Modular Tool-based analysis
- ToolChains for multi-step workflows
- External INI configuration
- Reusable shared C++ classes
- CMake-based build system

---

# Project Structure

```text
ToolFlow/
│
├── tools/
├── toolChains/
├── shared/
├── iniReader/
├── createTool
├── createToolChain
├── CMakeLists.txt
└── README.md
```

- **tools/** contains independent analysis Tools.
- **toolChains/** contains analysis workflows.
- **shared/** contains reusable classes shared between multiple Tools (e.g. experiment data structures, utility classes, and helper algorithms).
- **iniReader/** provides a lightweight INI configuration parser.
- **createTool** automatically generates a new Tool.
- **createToolChain** automatically generates a new ToolChain.

---

# Tool

A Tool is an independent executable that performs a specific analysis task.

Each Tool has

- its own source code
- its own executable
- its own configuration file
- its own CMake configuration

A Tool may consist of a single `main.cpp`, similar to a traditional ROOT macro, or it can be freely extended with additional source files, classes, and external libraries as the analysis grows.

---

# ToolChain

A ToolChain is a user-defined workflow that executes multiple Tools in sequence.

Rather than containing analysis code itself, a ToolChain simply specifies which Tools should be executed and which configuration files should be used.

This makes complex analysis workflows easy to build while keeping individual Tools independent and reusable.

---

# Communication Between Tools

ToolFlow does not enforce a particular communication mechanism between Tools.

For ROOT-based analyses, consecutive Tools typically communicate through ROOT files. A Tool reads an existing `TTree`, performs its analysis, stores newly computed quantities as additional branches, and writes the updated tree to a new ROOT file.

```text
Input ROOT
      │
      ▼
+-------------+
|   Tool A    |
+-------------+
      │
      ▼
Output ROOT
      │
      ▼
+-------------+
|   Tool B    |
+-------------+
      │
      ▼
Output ROOT
```

## Advantages

- Preserves intermediate results by storing the output of each Tool.
- Keeps Tools loosely coupled through ROOT files.
- Allows each Tool to be developed, tested, and debugged independently.
- Makes it easy to combine C++ and Python (e.g. using `uproot`) within the same analysis workflow.
- If a Tool in a ToolChain fails, previously completed Tools do not need to be rerun since their outputs are already available.

> **Note**
>
> Although exchanging data through intermediate ROOT files introduces additional disk I/O, this overhead is often modest compared to the computation performed by each Tool in typical offline analyses. The resulting modularity, reproducibility, and ability to inspect intermediate results frequently outweigh the additional I/O cost
---

# Helper Scripts

Create a new Tool

```bash
./createTool MyTool
```

The script automatically creates the Tool directory together with its initial source code, configuration file, CMakeLists, and README.

Create a new ToolChain

```bash
./createToolChain MyChain Tool1 Tool2 Tool3
```

The ToolChain definition and the corresponding Tool configuration files are created automatically.

Tool and ToolChain registries are updated automatically.

---

# Building

ToolFlow uses CMake and supports out-of-source builds.

Create a build directory

```bash
mkdir build
cd build
```

Build all Tools

```bash
cmake ../<source_dir>
cmake --build . -j
```

Build selected Tools

```bash
cmake ../<source_dir> \
    -DTOOLS="tools/convertWaveforms;tools/myTool"

cmake --build . -j
```

Build all Tools required by one or more ToolChains

```bash
cmake ../<source_dir> \
    -DTOOLCHAINS="toolChains/myToolChain"

cmake --build . -j
```

Multiple ToolChains can also be specified

```bash
cmake ../<source_dir> \
    -DTOOLCHAINS="toolChains/chain1;toolChains/chain2"

cmake --build . -j
```

`TOOLS` and `TOOLCHAINS` may also be combined. In this case, ToolFlow builds the union of all specified Tools and all Tools required by the selected ToolChains.

```bash
cmake ../<source_dir> \
    -DTOOLS="tools/myExtraTool" \
    -DTOOLCHAINS="toolChains/myToolChain"

cmake --build . -j
```

---

---

# Running

Run an individual Tool

```bash
./bin/myTool configs/myTool/config.ini
```

Run a ToolChain

```bash
./bin/runPipeline toolChains/myToolChain/myToolChain.tchain
```

---

# Examples

The original ToolFlow example Tools (`tool1`–`tool5`, `toolChain1`) were removed
from this repository once the first ENRG Tool existed. They are still available in
the [ToolFlow](https://github.com/mkandemirr/ToolFlow) repository and in the git
history of this repository. `tools/convertWaveforms` is the reference Tool here.
