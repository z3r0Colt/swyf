#pragma once
#include <functional>
#include <string>

// Plain C++ engine shared by the Android JNI wrapper and the desktop test harness.
namespace dialtone {

struct Stats {
    int prompt_tokens = 0;
    int reused_tokens = 0;
    int gen_tokens = 0;
    double prompt_ms = 0;
    double gen_ms = 0;
};

// Called with the reply text so far, after each new token.
using OnText = std::function<void(const std::string &)>;

// backend_dir: folder holding the ggml CPU backend libraries (Android), or "" when they are linked in.
bool load_model(const std::string &path, int threads, const std::string &backend_dir = "");
bool is_loaded();
// Prompt must already be in the model's chat format. Returns "" on failure.
// When force_tag is set and the reply has no "[...]" tag, generation continues with "\n[MOOD " so the
// game always gets a trust change.
std::string generate(const std::string &prompt, int max_tokens, bool force_tag, const OnText &on_text = nullptr);
Stats last_stats();

}
