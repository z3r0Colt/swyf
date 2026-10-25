#include "dialtone-engine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <vector>
#include "ggml-backend.h"
#include "llama.h"

namespace {
std::mutex guard;
llama_model *g_model = nullptr;
llama_context *g_ctx = nullptr;
// Saved model memory right after the previous prompt. A call only grows by the last reply and the new
// player line, so when the old prompt is a prefix of the new one only the new tokens are decoded.
std::vector<llama_token> g_snap_tokens;
std::vector<uint8_t> g_snap;
int g_threads = 4;
dialtone::Stats g_stats;
constexpr int kCtx = 4096;
constexpr int kBatch = 256;
constexpr int kTagTokens = 8;

// The caller should only speak its own line. Cut generation if it starts writing the player's turn.
const char *kStops[] = {"<|im_end|>", "<|im_start|>", "\nGeorge:", "\nGEORGE:", "\nUser:"};

size_t stop_at(const std::string &text) {
    size_t cut = std::string::npos;
    for (const char *s : kStops) cut = std::min(cut, text.find(s));
    return cut;
}

double ms_since(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}

std::vector<llama_token> tokenize(const llama_vocab *vocab, const std::string &text, bool special) {
    int n = -llama_tokenize(vocab, text.data(), (int) text.size(), nullptr, 0, special, special);
    if (n <= 0) return {};
    std::vector<llama_token> tokens((size_t) n);
    if (llama_tokenize(vocab, text.data(), (int) text.size(), tokens.data(), n, special, special) != n) return {};
    return tokens;
}

bool decode_all(llama_context *ctx, std::vector<llama_token> &tokens) {
    for (size_t p = 0; p < tokens.size(); p += kBatch) {
        int n = (int) std::min<size_t>(kBatch, tokens.size() - p);
        if (llama_decode(ctx, llama_batch_get_one(tokens.data() + p, n)) != 0) return false;
    }
    return true;
}

// Samples until end-of-generation, a stop string, `until` appearing, or the token limit.
// Returns false if decoding failed.
bool sample_into(llama_context *ctx, llama_sampler *sampler, const llama_vocab *vocab, std::string &out,
                 size_t from, int limit, const char *until, const dialtone::OnText &on_text) {
    for (int i = 0; i < limit; ++i) {
        llama_token tok = llama_sampler_sample(sampler, ctx, -1);
        if (llama_vocab_is_eog(vocab, tok)) return true;
        char piece[256];
        int n = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
        if (n > 0) out.append(piece, (size_t) n);
        g_stats.gen_tokens++;
        size_t cut = stop_at(out.substr(from));
        if (cut != std::string::npos) { out.resize(from + cut); return true; }
        if (on_text) on_text(out);
        if (until && out.find(until, from) != std::string::npos) return true;
        if (llama_decode(ctx, llama_batch_get_one(&tok, 1)) != 0) return false;
    }
    return true;
}

void quiet_log(ggml_log_level level, const char *text, void *) {
    if (level >= GGML_LOG_LEVEL_WARN) std::fputs(text, stderr);
}

// Brings g_ctx to the state after `tokens`, reusing the snapshot when possible. Returns false on failure.
bool prime(const std::vector<llama_token> &tokens) {
    llama_memory_t mem = llama_get_memory(g_ctx);
    size_t start = 0;
    bool reuse = !g_snap.empty() && g_snap_tokens.size() < tokens.size() &&
                 std::equal(g_snap_tokens.begin(), g_snap_tokens.end(), tokens.begin());
    llama_memory_clear(mem, true);
    if (reuse && llama_state_seq_set_data(g_ctx, g_snap.data(), g_snap.size(), 0) > 0) start = g_snap_tokens.size();
    else llama_memory_clear(mem, true);
    g_snap.clear();
    g_snap_tokens.clear();
    g_stats.reused_tokens = (int) start;

    std::vector<llama_token> rest(tokens.begin() + (long) start, tokens.end());
    if (!decode_all(g_ctx, rest)) return false;
    g_snap.resize(llama_state_seq_get_size(g_ctx, 0));
    if (llama_state_seq_get_data(g_ctx, g_snap.data(), g_snap.size(), 0) == g_snap.size()) g_snap_tokens = tokens;
    else g_snap.clear();
    return true;
}
}

namespace dialtone {

bool is_loaded() {
    std::lock_guard<std::mutex> lk(guard);
    return g_model != nullptr;
}

Stats last_stats() {
    std::lock_guard<std::mutex> lk(guard);
    return g_stats;
}

bool load_model(const std::string &path, int threads, const std::string &backend_dir) {
    std::lock_guard<std::mutex> lk(guard);
    if (g_model) return true;
    if (path.empty()) return false;
    llama_log_set(quiet_log, nullptr);
    if (!backend_dir.empty()) ggml_backend_load_all_from_path(backend_dir.c_str());
    llama_backend_init();
    g_threads = std::max(1, std::min(8, threads));
    auto params = llama_model_default_params();
    params.n_gpu_layers = 0; // CPU inference, no GPU driver dependencies
    g_model = llama_model_load_from_file(path.c_str(), params);
    if (!g_model) return false;

    auto cp = llama_context_default_params();
    cp.n_ctx = kCtx;
    cp.n_batch = kBatch;
    cp.n_ubatch = kBatch;
    cp.n_threads = g_threads;
    cp.n_threads_batch = g_threads;
    g_ctx = llama_init_from_model(g_model, cp);
    if (!g_ctx) { llama_model_free(g_model); g_model = nullptr; }
    return g_model != nullptr;
}

std::string generate(const std::string &prompt, int requested, bool force_tag, const OnText &on_text) {
    std::lock_guard<std::mutex> lk(guard);
    g_stats = Stats{};
    if (!g_model || !g_ctx) return {};
    llama_context *ctx = g_ctx;
    const llama_vocab *vocab = llama_model_get_vocab(g_model);
    const int max_out = std::max(1, std::min(256, requested));

    std::vector<llama_token> tokens = tokenize(vocab, prompt, true);
    if (tokens.empty() || (int) tokens.size() >= kCtx - max_out - kTagTokens - 4) return {};

    llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(llama_vocab_n_tokens(vocab), 64, 1.1f, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string result;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = prime(tokens);
    g_stats.prompt_tokens = (int) tokens.size();
    g_stats.prompt_ms = ms_since(t0);

    auto t1 = std::chrono::steady_clock::now();
    if (ok) ok = sample_into(ctx, sampler, vocab, result, 0, max_out, nullptr, on_text);
    if (ok && force_tag && result.find('[') == std::string::npos) {
        // Whatever ended the line was never decoded, so the context ends right after the last spoken token.
        while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) result.pop_back();
        // Only the mood is forced. A forced "[" alone makes small models pick HANG UP or PAY at random.
        std::vector<llama_token> tag = tokenize(vocab, "\n[MOOD ", false);
        result += "\n[MOOD ";
        if (!tag.empty() && decode_all(ctx, tag)) sample_into(ctx, sampler, vocab, result, result.size(), kTagTokens, "]", nullptr);
    }
    g_stats.gen_ms = ms_since(t1);

    llama_sampler_free(sampler);
    return result;
}

}
