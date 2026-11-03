cmake_minimum_required(VERSION 3.22)
project(AbyssTone VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: use ./JUCE if present, otherwise download it automatically
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(ABYSS_FORMATS VST3 AU Standalone)
else()
    set(ABYSS_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AbyssTone
    COMPANY_NAME "Abyss Audio"
    BUNDLE_ID com.abyssaudio.abysstone
    PLUGIN_MANUFACTURER_CODE Abys
    PLUGIN_CODE Abt1
    FORMATS ${ABYSS_FORMATS}
    PRODUCT_NAME "Abyss Tone"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Abyss Tone needs audio input to process your guitar."
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(AbyssTone PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AbyssTone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(ABYSS_BUILD_TESTS "Build offline render test" OFF)
if(ABYSS_BUILD_TESTS)
    juce_add_console_app(AbyssRenderTest PRODUCT_NAME "AbyssRenderTest")
    target_sources(AbyssRenderTest PRIVATE Tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(AbyssRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Abyss Tone" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AbyssRenderTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags)
endif()

target_link_libraries(AbyssTone
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
