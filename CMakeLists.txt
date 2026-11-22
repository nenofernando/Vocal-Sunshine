cmake_minimum_required(VERSION 3.22)
project(NFVocalSunshine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
option(NFVocalSunshine_BUILD_TOOLS "Build the UI snapshot and smoke-test tools" OFF)

find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
 FetchContent_MakeAvailable(JUCE)
endif()

set(NFVocalSunshine_FORMATS VST3)
if(APPLE)
 list(APPEND NFVocalSunshine_FORMATS AU)
endif()

juce_add_binary_data(NFVocalSunshineAssets HEADER_NAME NFVocalSunshineBinaryData.h NAMESPACE NFVocalSunshineBinaryData
 SOURCES Assets/NF_Audio_Tools_Logo_MAIN_800.png)

set(VS_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)
set(VS_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)

juce_add_plugin(NFVocalSunshine
 COMPANY_NAME "NF Audio Tools"
 COMPANY_COPYRIGHT "NF Audio Tools by Nenno Fernando. (C) 2026 NF Audio Tools. All rights reserved."
 BUNDLE_ID "com.nfaudiotools.nfvocalsunshine"
 PLUGIN_MANUFACTURER_CODE Nfat
 PLUGIN_CODE Nfsu
 FORMATS ${NFVocalSunshine_FORMATS}
 PRODUCT_NAME "NF Vocal Sunshine"
 PLUGIN_NAME "NF Vocal Sunshine"
 DESCRIPTION "Seven-stage guided vocal processor"
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
 VST3_CATEGORIES Fx
 AU_MAIN_TYPE kAudioUnitType_Effect)
juce_generate_juce_header(NFVocalSunshine)
target_sources(NFVocalSunshine PRIVATE ${VS_SOURCES})
target_compile_definitions(NFVocalSunshine PUBLIC ${VS_DEFS})
target_link_libraries(NFVocalSunshine PRIVATE NFVocalSunshineAssets juce::juce_audio_utils juce::juce_dsp
 PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(NFVocalSunshine_BUILD_TOOLS)
 foreach(tool Snapshot SmokeTest KeyTool)
  juce_add_console_app(NFVocalSunshine${tool} PRODUCT_NAME "NFVocalSunshine${tool}")
  juce_generate_juce_header(NFVocalSunshine${tool})
  target_sources(NFVocalSunshine${tool} PRIVATE Tests/${tool}.cpp ${VS_SOURCES})
  target_compile_definitions(NFVocalSunshine${tool} PRIVATE JucePlugin_Name="NF Vocal Sunshine" JucePlugin_VersionString="0.1.0"
   JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JucePlugin_Build_Standalone=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(NFVocalSunshine${tool} PRIVATE NFVocalSunshineAssets juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra juce::juce_recommended_config_flags)
 endforeach()
endif()
