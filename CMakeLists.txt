cmake_minimum_required (VERSION 3.18)

## Project Definition and Options ##

project(WinToast)


## Source Files ##

add_dir(WinToast src)


## Define libstrophe library ##

add_library(WinToast STATIC ${WinToast_SOURCES})


## Dependencies ##

target_include_directories(WinToast PUBLIC src)


## Properties ##

# C++20 #
set_property(TARGET WinToast PROPERTY CXX_STANDARD 20)

# Add .pdb for release builds #
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_BUILD_TYPE MATCHES "Release")
   target_compile_options(WinToast PRIVATE /Zi)
   set_target_properties(WinToast PROPERTIES
        LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
        COMPILE_PDB_NAME WinToast 
        COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
   )
endif()
