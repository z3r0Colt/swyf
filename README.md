# DIALTONE: offline AI caller for Android

A phone-sized chat game in the spirit of Scam With Your Friends. You play George at a shady call center, and the person on the other end of the line is a small language model running **entirely on the phone**. No internet, no API key, no server.

The model is **Qwen3.5-2B** (Apache 2.0), quantized to Q4_K_M (1.28 GB GGUF from `unsloth/Qwen3.5-2B-GGUF`), run with `llama.cpp` (MIT, pinned to tag `b11869`). It is bundled inside the APK, so the APK is about 1.3 GB.

## How the caller works

- `app/src/main/assets/caller-ai.js` holds the four customers (name, backstory, what George is "selling") and builds the chat prompt. The model replies in character, then adds a tag such as `[MOOD +5]`, `[HANG UP]` or `[PAY]`.
- `app.js` turns those tags into the trust meter. Trust and patience decide whether the customer hangs up, pays, or the call times out. A few hard rules (threats, insults, admitting it's a scam) always override the model.
- `app/src/main/cpp/dialtone-engine.cpp` runs the model. It streams the reply as it is written, so the customer "types" live. It also keeps its memory of the call between turns, so each turn only reads the new lines instead of the whole conversation again.
- `MainActivity.java` copies the model out of the APK into private storage on first launch (shows `UNPACKING n%`), then loads it.
- The engine ships several CPU builds (`libggml-cpu-android_armv8.0` up to `armv9.2`) and picks the fastest one the phone supports at runtime.

Phone requirements: 64-bit ARM Android 8.0+, about 3 GB free storage (APK plus the unpacked model), and 6 GB+ RAM recommended.

## Build with GitHub Actions (easiest)

1. Open the **Actions** tab, pick **Build DIALTONE bundled-AI APK**, and press **Run workflow**.
2. It downloads the model on GitHub's machine, builds the APK, and checks that `assets/model.gguf` is really inside it.
3. Download the `DIALTONE-offline-AI-APK` artifact, unzip it, and install the APK on your phone.

The model file is never committed to git. It is fetched at build time.

## Build locally

Install Android SDK platform 35, NDK `27.2.12479018`, CMake `3.22.1`, Java 17, Python 3, Git and Gradle 8.11+. Then from the project root:

```sh
bash tools/setup_engine.sh       # fetches llama.cpp source
python3 tools/fetch_model.py     # downloads the model into app/src/main/assets/
gradle assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`. To use a GGUF you already have, set `DIALTONE_MODEL_PATH=/path/to/file.gguf` before `fetch_model.py`. The Gradle build refuses to produce an APK without a real model.

## Testing the caller on a computer

The engine also builds as a desktop program, so you can tune the customers' prompts without a phone:

```sh
cmake -S app/src/main/cpp -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --target dialtone-chat-test
node tools/chat-test.js build-host/dialtone-chat-test app/src/main/assets/model.gguf
```

It plays a few scripted calls through the same `caller-ai.js` the app uses and prints each reply, its mood tag, and timings.

## Swapping the model

Any chat GGUF that `llama.cpp` supports will load. If you switch away from Qwen, update the chat format in `buildPrompt` in `caller-ai.js` (the `<|im_start|>` markers and the empty `<think>` block are Qwen-specific) and the URL in `tools/fetch_model.py`.
