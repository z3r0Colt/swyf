# DIALTONE: Android APK with model bundled inside

This project upgrades the DIALTONE chat game to an **offline, native Android application** backed by a locally-running Qwen3-1.7B Q4_K_M model and `llama.cpp`. No API key, external AI provider, hosting, or model download **on the phone** is used. The quantized GGUF model is embedded in `assets/model.gguf` **during the APK build**. The APK will be around 1.3 GB. First launch extracts the model once to private app storage and loads it, so allow 2.5–3 GB of free device storage in addition to memory for inference. A 64-bit ARM Android phone with at least 6–8 GB RAM is recommended. Performance varies widely by device.

## IMPORTANT: This ZIP is BUILD SOURCE, not a completed model-inclusive APK

The model is **not included in this source ZIP**, because the build environment used to prepare it cannot download the ~1.28 GB GGUF weights and does not have the Android SDK/NDK/Gradle. Therefore it has **not been built or device-tested**. Do not mistake the earlier tiny `DIALTONE-Android.apk` for this build; that older file does not contain a model.

### Easiest build: GitHub Actions

1. Create a private GitHub repository and upload the *contents* of this ZIP, including `.github/workflows/build-apk.yml`.
2. Open the **Actions** tab and run **Build DIALTONE bundled-AI APK** with **Run workflow**.
3. The workflow downloads the ~1.28 GB open-weight model on the GitHub build machine, fetches pinned `llama.cpp` source, compiles a native ARM64 app, verifies that `assets/model.gguf` is **actually present inside the APK**, and uploads an APK build artifact.
4. Download the artifact ZIP from the workflow run. Extract the APK and install it on your Android phone.

No model download occurs on the phone. A development-signed APK may require uninstalling the earlier version because the signing key differs. Your old on-device save will be removed if you uninstall that APK.

### Local build with Android Studio

Install Android Studio, Android SDK API 35, NDK `27.2.12479018`, CMake `3.22.1`, Java 17, Python 3, Git, and Gradle 8.11.1. From this project root:

```sh
bash tools/setup_engine.sh
python3 tools/fetch_model.py
gradle assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`.

You may alternatively supply an existing locally downloaded Q4 GGUF (not another incompatible architecture) by setting `DIALTONE_MODEL_PATH=/path/to/file.gguf` before running the fetch script. The Gradle build **refuses to produce an APK** if `model.gguf` is missing or suspiciously small.

### Offline AI architecture

- Pure Java Android Activity wraps the original mobile-first WebView game.
- JavaScript calls a scoped `DialtoneNative` interface exposed only inside the application's embedded HTML.
- Java extracts bundled `assets/model.gguf` into private app storage on the first launch, then native JNI loads it.
- Native `llama.cpp` runs quantized ARM64 CPU inference with Qwen3's non-thinking chat prompt format.
- Scripted rules separately manage trust, patience, quotas, and call outcomes. The model writes the caller's fictional conversation, making responses more varied.
- Campaign saves use the private WebView's localStorage.
- The app requests no network permissions, and it never sends conversations off-device.

### Licensing and technical notes

Qwen3-1.7B weights are licensed Apache 2.0 by Qwen. The quantized GGUF is attributed to `bartowski/Qwen_Qwen3-1.7B-GGUF` on Hugging Face. Native inference uses `ggml-org/llama.cpp` (MIT), pinned to tag `b11869`. Review each upstream license when distributing builds. The package is a personal project inspired by a game genre, not a copy of someone else's game artwork or assets.

The source and workflow have been prepared and the JavaScript syntax checked. Android compilation and on-device inference are **not verified** here. API or device-specific fixes could be needed. A full APK cannot truthfully be presented until the model weights and Android native build have completed.
