cmake_minimum_required(VERSION 3.22)
project(umi_sampler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Build options you can flip later
option(UMI_WITH_DSP "Include juce_dsp" OFF)
option(UMI_WITH_AUDIO_FORMATS "Enable WAV/AIFF loaders" ON)

# JUCE
add_subdirectory(external/JUCE)

# Headless console app (good for Pi services)
juce_add_console_app(umi_sampler
    PRODUCT_NAME "UMI Sampler"
    VERSION      "${PROJECT_VERSION}"
)

# Tell JUCE note to use libcurl
target_compile_definitions(umi_sampler PRIVATE JUCE_USE_CURL=0)

target_sources(umi_sampler PRIVATE
    src/main.cpp
    src/Engine.h
    src/Engine.cpp
    src/SamplerEngine.h
    src/SamplerEngine.cpp
    src/SerialLinux.h
    src/SerialService.h
)

# Lean module set for audio + osc + (later) sample loading
target_link_libraries(umi_sampler PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_osc
)

if(UMI_WITH_AUDIO_FORMATS)
    target_link_libraries(umi_sampler PRIVATE juce::juce_audio_formats)
endif()
if(UMI_WITH_DSP)
    target_link_libraries(umi_sampler PRIVATE juce::juce_dsp)
endif()

# Common defs: quiet, strict, no splash
target_compile_definitions(umi_sampler PRIVATE
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Linux/Raspberry Pi specifics
if(UNIX AND NOT APPLE)
    target_compile_definitions(umi_sampler PRIVATE
        JUCE_ALSA=1
        JUCE_USE_X11=0   # headless: no X dependencies on Pi
    )
endif()

# Release optimisations (compiler-appropriate)
if(MSVC)
    target_compile_options(umi_sampler PRIVATE $<$<CONFIG:Release>:/O2 /fp:fast>)
else()
    target_compile_options(umi_sampler PRIVATE $<$<CONFIG:Release>:-O3 -ffast-math>)
endif()

set_target_properties(umi_sampler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)