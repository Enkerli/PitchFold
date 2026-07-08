cmake_minimum_required(VERSION 3.22)
project(PitchFold VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# ── Suite foundation (enkerli-juce submodule): JUCE resolution + archetypes ──
include("${CMAKE_CURRENT_SOURCE_DIR}/enkerli-juce/cmake/EnkerliPlugin.cmake")
enkerli_resolve_juce()

# ── Platform detection ────────────────────────────────────────────────────────
set(IS_IOS FALSE)
set(IS_MACOS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
    message(STATUS "Configuring for iOS")
elseif(APPLE)
    set(IS_MACOS TRUE)
    message(STATUS "Configuring for macOS")
endif()

if(IS_IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS version")
elseif(IS_MACOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# ── Plugin (aumi archetype — the known-good MIDI-effect settings) ────────────
# BUNDLE_ID stays "com.enkerli.PitchFold" (capital P): installed devices have
# it; the archetype's lowercase default would orphan them.
enkerli_add_midi_effect_plugin(PitchFold
    PLUGIN_CODE    Pqf1
    BUNDLE_ID      "com.enkerli.PitchFold"
    VERSION        "${PROJECT_VERSION}"
    DESCRIPTION    "PCS pitch quantizer with performance pads"
    ICON_BIG       "${CMAKE_SOURCE_DIR}/Assets/icon_1024.png"
    PLIST_TO_MERGE "${CMAKE_SOURCE_DIR}/Assets/Info-Standalone.plist"
)

add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-finput-charset=UTF-8>)

# ── Bundled fonts ─────────────────────────────────────────────────────────────
juce_add_binary_data(PitchFoldFonts SOURCES
    Assets/Fonts/Domine.ttf
    Assets/Fonts/InterTight.ttf
    Assets/Fonts/InterTight-Italic.ttf
    Assets/Fonts/Caveat.ttf
)
target_link_libraries(PitchFold PRIVATE PitchFoldFonts)

# ── WebUI bundle ──────────────────────────────────────────────────────────────
# The WebUI *source* lives in the music-suite monorepo (apps/pitchfold) — single
# source shared with the standalone webapp; only the C++ editor (PitchFoldEditor.*)
# stays here. Default to the maintainer's sibling checkout; override per machine
# in webui.local.cmake (gitignored). esbuild/react resolve from the monorepo's
# hoisted node_modules, so the monorepo must be checked out and `npm install`ed.
set(PITCHFOLD_WEBUI_DIR "${CMAKE_SOURCE_DIR}/../music-suite/apps/pitchfold"
    CACHE PATH "Path to the PitchFold WebUI source (music-suite/apps/pitchfold)")
include("${CMAKE_CURRENT_SOURCE_DIR}/webui.local.cmake" OPTIONAL)
set(WEBUI_DIR  "${PITCHFOLD_WEBUI_DIR}")
set(WEBUI_DIST "${CMAKE_CURRENT_BINARY_DIR}/webui")

if(NOT EXISTS "${WEBUI_DIR}/main.jsx")
    message(FATAL_ERROR
        "PitchFold WebUI source not found at ${WEBUI_DIR}. It now lives in the "
        "music-suite monorepo (apps/pitchfold). Check out music-suite next to this "
        "repo, or set PITCHFOLD_WEBUI_DIR in webui.local.cmake.")
endif()

find_program(NPXCMD npx)
if(NPXCMD)
    # esbuild produces bundle.js only; index.html is embedded from source (it's
    # a static shell). A single-output custom command avoids the Visual Studio
    # generator's multi-output/one-rule confusion (MSB8065 → juceaide crash).
    add_custom_command(
        OUTPUT  "${WEBUI_DIST}/bundle.js"
        COMMAND "${NPXCMD}" esbuild "${WEBUI_DIR}/main.jsx"
                    --bundle --conditions=source
                    "--inject:${WEBUI_DIR}/react-globals.js"
                    "--banner:js=/*! Bundles WEBMIDI.js (c) Jean-Philippe Cote, Apache-2.0, https://webmidijs.org - see @enkerli/webmidi/THIRD_PARTY_LICENSES.md */"
                    "--define:process.env.NODE_ENV=\"production\""
                    --minify
                    "--outfile=${WEBUI_DIST}/bundle.js"
        DEPENDS
            "${WEBUI_DIR}/main.jsx"
            "${WEBUI_DIR}/juce-bridge.js"
            "${WEBUI_DIR}/webmidi-bridge.js"
            "${WEBUI_DIR}/react-globals.js"
            "${WEBUI_DIR}/index.html"
            "${WEBUI_DIR}/engine/pcs.js"
            "${WEBUI_DIR}/engine/voices.js"
            "${WEBUI_DIR}/design/tokens.jsx"
            "${WEBUI_DIR}/design/ui-primitives.jsx"
            "${WEBUI_DIR}/design/scale-editor.jsx"
            "${WEBUI_DIR}/design/pcs-explorer.jsx"
            "${WEBUI_DIR}/design/quantizer-panel.jsx"
            "${WEBUI_DIR}/design/chord-pads.jsx"
            "${WEBUI_DIR}/design/voice-panel.jsx"
            "${WEBUI_DIR}/design/time-panel.jsx"
        WORKING_DIRECTORY "${WEBUI_DIR}"
        COMMENT "Bundling PitchFold WebUI with esbuild"
        VERBATIM
    )
    add_custom_target(PitchFoldWebUIBundle
        DEPENDS "${WEBUI_DIST}/bundle.js")
endif()

# The generated bundle.js is marked GENERATED so the VS generator schedules it
# after the custom command; index.html is embedded straight from source.
set_source_files_properties("${WEBUI_DIST}/bundle.js" PROPERTIES GENERATED TRUE)
juce_add_binary_data(PitchFoldWebUI
    HEADER_NAME BinaryDataWebUI.h
    NAMESPACE   BinaryData
    SOURCES
        "${WEBUI_DIR}/index.html"
        "${WEBUI_DIST}/bundle.js"
)
if(NPXCMD)
    add_dependencies(PitchFoldWebUI PitchFoldWebUIBundle)
endif()
target_link_libraries(PitchFold PRIVATE PitchFoldWebUI)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(PitchFold PRIVATE
    Source/PluginProcessor.cpp
    Source/WebUI/PitchFoldEditor.cpp
    Source/StandaloneApp.cpp
    # Header-only — shown in Xcode navigator
    Source/PluginProcessor.h
    Source/PCS/ScaleData.h
    Source/PCS/PCSEngine.h
    Source/Quantizer/DelayBuffer.h
    Source/Quantizer/TimeQuantizer.h
    Source/Quantizer/VoiceProcessor.h
    Source/Pads/ChordPadBank.h
    Source/UI/ChromaticWheel.h
    Source/UI/ScaleLattice.h
    Source/UI/PCSPreview.h
)

target_include_directories(PitchFold PRIVATE
    Source Source/PCS Source/Quantizer Source/Pads Source/UI Source/WebUI)

# ── Compile definitions (suite-standard ones come from the archetype) ────────
target_compile_definitions(PitchFold
    PUBLIC
        JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0
)

if(IS_MACOS)
    target_compile_definitions(PitchFold PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()

target_link_libraries(PitchFold
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
