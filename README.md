# PitchFold

A quantizer based on Pitch Class Sets (preliminary work). JUCE plugin with a
WebView UI; the PCS picker is planned to embed
[PickPCS](https://github.com/Enkerli/PickPCS) — see [DESIGN.md](DESIGN.md).

## Building

```bash
npm --prefix Source/WebUI install   # WebUI bundle dependencies (once)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PitchFold_AU   # or PitchFold_VST3, PitchFold_Standalone
```

JUCE is resolved in this order:

1. An installed JUCE package (`find_package`)
2. A local `JUCE/` checkout or symlink (or `-DJUCE_PATH=/path/to/JUCE`) —
   fastest for development: `ln -s /Applications/JUCE JUCE`
3. Automatic download via FetchContent, pinned to JUCE 8.0.13 — a clean clone
   builds with no setup

## License

See [LICENSE](LICENSE).
