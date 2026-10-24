cmake_minimum_required(VERSION 3.10)
project(ENRGToolFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# 1) ROOT'u bul
# ------------------------------------------------------------
find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

# ------------------------------------------------------------
# 2) shared/ altındaki kaynaklardan kütüphane oluştur (dictionary YOK)
# ------------------------------------------------------------
set(SHARED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/shared/include")
set(SHARED_SRC_DIR     "${CMAKE_SOURCE_DIR}/shared/src")
set(INIREADER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/iniReader")

file(GLOB_RECURSE SHARED_SOURCES "${SHARED_SRC_DIR}/*.cc")

add_library(MySharedLib SHARED
  ${SHARED_SOURCES}
)

target_include_directories(MySharedLib
  PUBLIC
    ${SHARED_INCLUDE_DIR}
)

target_link_libraries(MySharedLib
  PUBLIC
    ${ROOT_LIBRARIES}
    ROOT::Spectrum
    ROOT::Minuit
)



# ------------------------------------------------------------
# Helper function for building tools
# ------------------------------------------------------------
function(addTool toolName)

    add_executable(${toolName} main.cpp)

    target_include_directories(${toolName} PRIVATE
        ${INIREADER_INCLUDE_DIR}
    )

    target_link_libraries(${toolName} PRIVATE
        MySharedLib
        ${ROOT_LIBRARIES}
    )

    set_target_properties(${toolName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    set_target_properties(MySharedLib PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
   )

    # Config dosyalarını kopyala
    set(CONFIG_OUTPUT_DIR ${CMAKE_BINARY_DIR}/configs/${toolName})
    file(MAKE_DIRECTORY ${CONFIG_OUTPUT_DIR})

    file(GLOB CONFIG_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.mac"
    )

    foreach(cfg_file ${CONFIG_FILES})
        configure_file(
            ${cfg_file}
            ${CONFIG_OUTPUT_DIR}/
            COPYONLY
        )
    endforeach()

endfunction()

# ------------------------------------------------------------
# 3) Select Tools / ToolChains to compile
# ------------------------------------------------------------

set(TOOLS "" CACHE STRING
    "List of tools to build, e.g. tools/tool1;tools/tool2. Empty = all tools unless TOOLCHAINS is set.")

set(TOOLCHAINS "" CACHE STRING
    "List of toolChains to build, e.g. toolChains/chain1;toolChains/chain2.")

# Final list of tools that will be compiled
set(SELECTED_TOOLS "")

# ------------------------------------------------------------
# If TOOLS is given explicitly, add them directly.
# ------------------------------------------------------------
if(NOT TOOLS STREQUAL "")
    foreach(tool_path IN LISTS TOOLS)

        if(NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${tool_path}")
            message(FATAL_ERROR "Requested Tool directory does not exist: ${tool_path}")
        endif()

        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${tool_path}/CMakeLists.txt")
            message(FATAL_ERROR "Requested Tool has no CMakeLists.txt: ${tool_path}")
        endif()

        list(APPEND SELECTED_TOOLS "${tool_path}")

    endforeach()
endif()

# ------------------------------------------------------------
# If TOOLCHAINS is given, read tools.list from each ToolChain.
# ------------------------------------------------------------
if(NOT TOOLCHAINS STREQUAL "")

    foreach(chain_path IN LISTS TOOLCHAINS)

        set(tools_list_file "${CMAKE_SOURCE_DIR}/${chain_path}/tools.list")

        if(NOT EXISTS "${tools_list_file}")
            message(FATAL_ERROR "tools.list not found for ToolChain: ${chain_path}")
        endif()

        message(STATUS ">>> Reading ToolChain: ${chain_path}")

        file(STRINGS "${tools_list_file}" chain_tools)

        foreach(tool_name IN LISTS chain_tools)

            # Skip empty lines
            if(tool_name STREQUAL "")
                continue()
            endif()

            # tools.list contains tool names such as filterCCEvent
            # Convert them to tools/filterCCEvent
            list(APPEND SELECTED_TOOLS "tools/${tool_name}")

        endforeach()

    endforeach()

endif()

# Remove duplicate tools
list(REMOVE_DUPLICATES SELECTED_TOOLS)

# ------------------------------------------------------------
# Find all available tools.
# ------------------------------------------------------------
file(GLOB TOOL_DIRS RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_SOURCE_DIR}/tools/*")

# ------------------------------------------------------------
# Build decision:
#
# 1) If neither TOOLS nor TOOLCHAINS is given:
#       build all tools.
#
# 2) If TOOLS or TOOLCHAINS is given:
#       build only tools in SELECTED_TOOLS.
# ------------------------------------------------------------
foreach(tool_dir IN LISTS TOOL_DIRS)

    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${tool_dir}")

        if(TOOLS STREQUAL "" AND TOOLCHAINS STREQUAL "")
            set(build_this_tool TRUE)
        else()
            list(FIND SELECTED_TOOLS "${tool_dir}" idx)

            if(idx GREATER -1)
                set(build_this_tool TRUE)
            else()
                set(build_this_tool FALSE)
            endif()
        endif()

        if(build_this_tool)
            message(STATUS ">>> Adding tool: ${tool_dir}")
            add_subdirectory("${tool_dir}")
        else()
            message(STATUS "    Skipping tool: ${tool_dir}")
        endif()

    endif()

endforeach()


##----------------------------------------------------------------------------
## Copy all scripts to the build directory
##
#set(EXAMPLEB1_SCRIPTS
#  LAPPDInfo.txt
#  MCLAPPDInfo.txt
#  MCLAPPDInfoOriginal.txt
#  MRDInfo.txt
#  PMTInfo.txt
#  )

#foreach(_script ${EXAMPLEB1_SCRIPTS})
#  configure_file(
#    ${PROJECT_SOURCE_DIR}/${_script}
#    ${PROJECT_BINARY_DIR}/${_script}
#    COPYONLY
#    )
#endforeach()

#----------------------------------------------------------------------------
# Copy ToolChains directory/configs conditionally
#
set(TOOLCHAINS_OUTPUT_DIR ${PROJECT_BINARY_DIR}/toolChains)

# Case 1:
# No TOOLS and no TOOLCHAINS given:
# copy all ToolChains
if(TOOLS STREQUAL "" AND TOOLCHAINS STREQUAL "")

    message(STATUS ">>> Copying all ToolChains")

    file(COPY
        ${PROJECT_SOURCE_DIR}/toolChains
        DESTINATION ${PROJECT_BINARY_DIR}
    )

# Case 2:
# TOOLCHAINS is given:
# copy only selected ToolChains
elseif(NOT TOOLCHAINS STREQUAL "")

    file(MAKE_DIRECTORY ${TOOLCHAINS_OUTPUT_DIR})

    foreach(chain_path IN LISTS TOOLCHAINS)

        if(NOT IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${chain_path}")
            message(FATAL_ERROR "Requested ToolChain directory does not exist: ${chain_path}")
        endif()

        message(STATUS ">>> Copying selected ToolChain: ${chain_path}")

        file(COPY
            ${PROJECT_SOURCE_DIR}/${chain_path}
            DESTINATION ${TOOLCHAINS_OUTPUT_DIR}
        )

    endforeach()

# Case 3:
# TOOLS is given but TOOLCHAINS is empty:
# do not copy ToolChains
else()

    message(STATUS ">>> TOOLS selected. No ToolChains copied.")

endif()



