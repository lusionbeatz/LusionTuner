cmake_minimum_required(VERSION 3.22)

# ─────────────────────────────────────────────────────────────────────────────
# Project
# ─────────────────────────────────────────────────────────────────────────────
project(LusionTuner VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────────────────────────
# Locate JUCE
#   Priority:
#     1. -DJUCE_PATH=<path>  (set by CI workflow)
#     2. JUCE/ subfolder inside the repo  (developer convenience)
#     3. System-installed JUCE (e.g. Homebrew /usr/local/share/JUCE)
# ─────────────────────────────────────────────────────────────────────────────
if(DEFINED JUCE_PATH)
    message(STATUS "Using JUCE from: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "Using bundled JUCE/")
    add_subdirectory(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
    message(STATUS "Using system JUCE: ${JUCE_VERSION}")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Plugin target
# ─────────────────────────────────────────────────────────────────────────────
juce_add_plugin(LusionTuner
    # ── Identity ────────────────────────────────────────────────────────────
    PRODUCT_NAME        "LusionTuner"
    COMPANY_NAME        "LusionBeatz"
    COMPANY_WEBSITE     "https://lusionbeatz.com"
    COMPANY_EMAIL       "info@lusionbeatz.com"
    BUNDLE_ID           "com.lusionbeatz.lusiontuner"
    PLUGIN_MANUFACTURER_CODE "LsBt"
    PLUGIN_CODE         "LsTn"
    VERSION             "1.0.0"

    # ── Formats ─────────────────────────────────────────────────────────────
    # macOS CI builds VST3 + AU.
    # On Windows the CMake build also produces VST3 (VS project used for Windows).
    FORMATS             VST3 AU Standalone

    # ── Plugin characteristics ───────────────────────────────────────────────
    IS_SYNTH            TRUE
    NEEDS_MIDI_INPUT    TRUE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # ── VST3 categories ──────────────────────────────────────────────────────
    VST3_CATEGORIES     "Instrument" "Instrument|Synth" "Pitch Shift" "Sampler"

    # ── AU type  (kAudioUnitType_MusicDevice = instrument synth) ─────────────
    AU_MAIN_TYPE        kAudioUnitType_MusicDevice

    # ── Preprocessor ─────────────────────────────────────────────────────────
    PREPROCESSOR_DEFINITIONS
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# ─────────────────────────────────────────────────────────────────────────────
# Binary resources (logo.png → BinaryData)
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_SOURCE_DIR}/logo.png")
    juce_add_binary_data(LusionTuner_BinaryData
        SOURCES logo.png
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Source files  (DSP / UI untouched)
# ─────────────────────────────────────────────────────────────────────────────
target_sources(LusionTuner
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SamplerVoice.cpp
        Source/PitchDetector.cpp
        Source/DSPProcessor.cpp
        # Header-only / compile=0 files – listed so IDEs show them
        Source/PluginProcessor.h
        Source/PluginEditor.h
        Source/SamplerVoice.h
        Source/PitchDetector.h
        Source/DSPProcessor.h
        Source/KeyboardDisplay.h
        Source/LusionLookAndFeel.h
        Source/WaveformDisplay.h
)

# ─────────────────────────────────────────────────────────────────────────────
# Include paths
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(LusionTuner
    PRIVATE
        Source
        JuceLibraryCode    # keeps #include <JuceHeader.h> working
)

# ─────────────────────────────────────────────────────────────────────────────
# JUCE module dependencies  (mirrors .jucer MODULES block)
# ─────────────────────────────────────────────────────────────────────────────
target_link_libraries(LusionTuner
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        $<$<TARGET_EXISTS:LusionTuner_BinaryData>:LusionTuner_BinaryData>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─────────────────────────────────────────────────────────────────────────────
# macOS-specific flags
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    # Silence known JUCE deprecation warnings on newer macOS SDKs
    target_compile_options(LusionTuner PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-parameter
    )
    # Sign ad-hoc so the .component / .vst3 loads in unsigned dev environments
    set_target_properties(LusionTuner PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY   "-"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED  "NO"
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Windows-specific flags  (keeps Windows build working unchanged)
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(LusionTuner PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
    set_target_properties(LusionTuner PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
