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
set(WEBUI_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI")
set(WEBUI_DIST "${WEBUI_DIR}/dist")

find_program(NPXCMD npx)
if(NPXCMD)
    add_custom_command(
        OUTPUT  "${WEBUI_DIST}/bundle.js" "${WEBUI_DIST}/index.html"
        COMMAND "${NPXCMD}" esbuild "${WEBUI_DIR}/main.jsx"
                    --bundle
                    "--inject:${WEBUI_DIR}/react-globals.js"
                    "--define:process.env.NODE_ENV=\"production\""
                    --minify
                    "--outfile=${WEBUI_DIST}/bundle.js"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${WEBUI_DIR}/index.html" "${WEBUI_DIST}/index.html"
        DEPENDS
            "${WEBUI_DIR}/main.jsx"
            "${WEBUI_DIR}/juce-bridge.js"
            "${WEBUI_DIR}/react-globals.js"
            "${WEBUI_DIR}/index.html"
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
        DEPENDS "${WEBUI_DIST}/bundle.js" "${WEBUI_DIST}/index.html")
endif()

juce_add_binary_data(PitchFoldWebUI
    HEADER_NAME BinaryDataWebUI.h
    NAMESPACE   BinaryData
    SOURCES
        "${WEBUI_DIST}/index.html"
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
