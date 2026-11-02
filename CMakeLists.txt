cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
project(XEMX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

# Use a local JUCE checkout when given (-DJUCE_DIR=...), otherwise download JUCE 8.0.8.
set(JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(XEMX
    COMPANY_NAME "XEMX"
    PRODUCT_NAME "XEMX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Xemx
    PLUGIN_CODE Xmx1
    BUNDLE_ID com.xemx.xemx
    VST3_CATEGORIES Instrument Drum Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS AU VST3 Standalone)

# Factory patterns so the device plays something before a .emx is loaded.
juce_add_binary_data(XemxData SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/factory/FACTORY.EMX)

target_sources(XEMX PRIVATE
    Source/EmxData.cpp
    Source/SampleBank.cpp
    Source/Fx.cpp
    Source/Voices.cpp
    Source/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(XEMX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(XEMX
    PRIVATE XemxData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
