cmake_minimum_required(VERSION 3.22)
project(RoobarbPedal VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE-free DSP tests (no plugin framework needed): `ctest` after building target dsp_test.
include(CTest)
add_executable(dsp_test EXCLUDE_FROM_ALL tests/dsp_test.cpp)
target_include_directories(dsp_test PRIVATE Source/dsp)
add_test(NAME dsp COMMAND dsp_test)

option(RC_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT RC_BUILD_PLUGIN)
    return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(RC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RC_FORMATS AU)
endif()

juce_add_plugin(RoobarbPedal
    COMPANY_NAME "Scuzz Audio Labs"   # placeholder - change to your own name / label
    PRODUCT_NAME "Roobarb Fuzz"
    PLUGIN_MANUFACTURER_CODE Scuz
    PLUGIN_CODE Rbfz
    FORMATS ${RC_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RoobarbPedal PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(RoobarbPedal PRIVATE Source Source/dsp)

target_compile_definitions(RoobarbPedal PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RoobarbPedal
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
