cmake_minimum_required(VERSION 3.22)
project(PitchFold VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# ── JUCE ─────────────────────────────────────────────────────────────────────
# Resolution order:
#   1. Installed JUCE package (find_package)
#   2. Local JUCE/ checkout or symlink, or -DJUCE_PATH=/path/to/JUCE
#   3. FetchContent download, pinned (works from a clean clone with no setup)
find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
    set(JUCE_PATH "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE directory")
    if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
        add_subdirectory("${JUCE_PATH}" JUCE)
    else()
        message(STATUS "JUCE not found locally — fetching juce-framework/JUCE 8.0.13")
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.13
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

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

# ── Plugin properties ─────────────────────────────────────────────────────────
set(PLUGIN_BASE_PROPS
    COMPANY_NAME             "Enkerli"
    COMPANY_WEBSITE          "https://enkerli.com"
    BUNDLE_ID                "com.enkerli.PitchFold"
    PLUGIN_MANUFACTURER_CODE Enke
    PLUGIN_CODE              Pqf1
    PRODUCT_NAME             "PitchFold"
    ICON_BIG                 "${CMAKE_SOURCE_DIR}/Assets/icon_1024.png"
    DESCRIPTION              "PCS pitch quantizer with performance pads"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

if(IS_IOS)
    juce_add_plugin(PitchFold
        ${PLUGIN_BASE_PROPS}
        FORMATS AUv3 Standalone
        MICROPHONE_PERMISSION_ENABLED FALSE
        IPHONE_SCREEN_ORIENTATIONS
            UIInterfaceOrientationPortrait
            UIInterfaceOrientationPortraitUpsideDown
            UIInterfaceOrientationLandscapeLeft
            UIInterfaceOrientationLandscapeRight
        IPAD_SCREEN_ORIENTATIONS
            UIInterfaceOrientationPortrait
            UIInterfaceOrientationPortraitUpsideDown
            UIInterfaceOrientationLandscapeLeft
            UIInterfaceOrientationLandscapeRight
        BACKGROUND_AUDIO_ENABLED TRUE
        PLIST_TO_MERGE "${CMAKE_SOURCE_DIR}/Assets/Info-Standalone.plist"
    )
elseif(IS_MACOS)
    juce_add_plugin(PitchFold
        ${PLUGIN_BASE_PROPS}
        FORMATS AU AUv3 VST3 Standalone
    )
else()
    juce_add_plugin(PitchFold
        ${PLUGIN_BASE_PROPS}
        FORMATS VST3 Standalone
    )
endif()

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

# ── iOS code signing ──────────────────────────────────────────────────────────
if(IS_IOS)
    foreach(target PitchFold_AUv3 PitchFold_Standalone)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM   "P8W7XXJN6C"
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE    "Automatic"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development")
        endif()
    endforeach()
endif()

if(IS_MACOS)
    if(TARGET PitchFold_AUv3)
        set_target_properties(PitchFold_AUv3 PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
                "${CMAKE_SOURCE_DIR}/Assets/PitchFold_AUv3.entitlements")
    endif()
endif()

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(PitchFold
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
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
