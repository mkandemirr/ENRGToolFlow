#!/bin/bash

# ------------------------------------------------------------
# createTool
#
# Creates a new tool skeleton under the tools/ directory.
#
# Generated files:
#   - main.cpp
#   - CMakeLists.txt
#   - config.ini
#   - README.md
#
# The script also updates tools/toolRegistry.txt.
#
# Usage:
#   ./createTool.sh <toolName>
#
# Example:
#   ./createTool.sh computeBaseline
# ------------------------------------------------------------

# ------------------------------------------------------------
# Rebuild the tool registry file.
# ------------------------------------------------------------
updateToolRegistry()
{
    REGISTRY_FILE="tools/toolRegistry.txt"

    TOOL_COUNT=0

    # Count all tool directories.
    for DIR in tools/*; do
        if [ -d "$DIR" ]; then
            TOOL_COUNT=$((TOOL_COUNT + 1))
        fi
    done

    # Write the registry header.
    echo "Total tools: ${TOOL_COUNT}" > "$REGISTRY_FILE"
    echo >> "$REGISTRY_FILE"

    # List all available tools.
    for DIR in tools/*; do
        if [ -d "$DIR" ]; then
            echo "$(basename "$DIR")" >> "$REGISTRY_FILE"
        fi
    done
}

# Check that a tool name is provided.
if [ $# -lt 1 ]; then
    echo "Usage: ./createTool.sh <toolName>"
    exit 1
fi

# Make sure the script is executed from the project root.
if [ ! -d "tools" ] || [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the project root directory."
    exit 1
fi

# Tool name given by the user.
TOOL_NAME=$1

# Directory that will be created.
TOOL_DIR="tools/${TOOL_NAME}"

# Prevent overwriting an existing tool.
if [ -d "$TOOL_DIR" ]; then
    echo "Error: Tool already exists: $TOOL_DIR"
    exit 1
fi

# Create the tool directory.
mkdir -p "$TOOL_DIR"

# ------------------------------------------------------------
# Generate main.cpp.
# ------------------------------------------------------------
cat > "${TOOL_DIR}/main.cpp" <<EOF
#include <iostream>
#include <string>
#include "INIReader.h"

// Returns 0 on success, non-zero on failure.
int ${TOOL_NAME}(const INIReader& iniReader);

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      std::cerr << "Usage: ./bin/${TOOL_NAME} configs/${TOOL_NAME}/config.ini"
                << std::endl;
      return 1;
   }

   INIReader iniReader(argv[1]);

   if (iniReader.ParseError() != 0)
   {
      std::cerr << "Error: could not load config file: "
                << argv[1] << std::endl;
      return 1;
   }

   return ${TOOL_NAME}(iniReader);
}

int ${TOOL_NAME}(const INIReader& iniReader)
{
   return 0;
}
EOF

# ------------------------------------------------------------
# Generate CMakeLists.txt.
# ------------------------------------------------------------
cat > "${TOOL_DIR}/CMakeLists.txt" <<EOF
addTool(${TOOL_NAME})
EOF

# ------------------------------------------------------------
# Create an empty configuration file.
# ------------------------------------------------------------
touch "${TOOL_DIR}/config.ini"




# ------------------------------------------------------------
# Create README.md.
# ------------------------------------------------------------
cat > "${TOOL_DIR}/README.md" <<EOF
# ${TOOL_NAME}

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
\`\`\`bash
./bin/${TOOL_NAME} configs/${TOOL_NAME}/config.ini
\`\`\`

## Notes / known limitations
EOF



# ------------------------------------------------------------
# Update the tool registry.
# ------------------------------------------------------------
updateToolRegistry

# ------------------------------------------------------------
# Print a summary.
# ------------------------------------------------------------
echo
echo "Tool created successfully."
echo
echo "Created files:"
echo "  ${TOOL_DIR}/main.cpp"
echo "  ${TOOL_DIR}/CMakeLists.txt"
echo "  ${TOOL_DIR}/config.ini"
echo "  ${TOOL_DIR}/README.md"
echo "  tools/toolRegistry.txt"
