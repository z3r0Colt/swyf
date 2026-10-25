#include <jni.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include "llama.h"

static std::mutex guard;
static llama_model *g_model = nullptr;
static bool g_backend = false;

static std::string from_jstring(JNIEnv *env, jstring s) {
    if (!s) return {};
    const char *p = env->GetStringUTFChars(s, nullptr);
    if (!p) return {};
    std::string value(p);
    env->ReleaseStringUTFChars(s, p);
    return value;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_gentleking_dialtone_NativeLlama_load(JNIEnv *env, jclass, jstring jpath) {
    std::lock_guard<std::mutex> lk(guard);
    if (g_model) return JNI_TRUE;
    std::string path = from_jstring(env,jpath);
    if (path.empty()) return JNI_FALSE;
    if (!g_backend) { llama_backend_init(); g_backend = true; }
    auto params = llama_model_default_params();
    params.n_gpu_layers = 0; // ARM64 CPU inference, no driver dependencies
    params.load_mode = LLAMA_LOAD_MODE_MMAP;
    g_model = llama_model_load_from_file(path.c_str(), params);
    return g_model ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_gentleking_dialtone_NativeLlama_generate(JNIEnv *env, jclass, jstring jprompt, jint requested) {
    std::lock_guard<std::mutex> lk(guard);
    if (!g_model) return env->NewStringUTF("");
    std::string prompt = from_jstring(env,jprompt);
    const llama_vocab *vocab = llama_model_get_vocab(g_model);
    // Qwen3 non-thinking mode. The chat template is provided by the frontend.
    const int maxOut = std::max(1, std::min(256, (int) requested));
    const int nCtx = 3072;
    int required = -llama_tokenize(vocab, prompt.data(), (int)prompt.size(), nullptr, 0, true, true);
    if (required <= 0 || required >= nCtx - maxOut) return env->NewStringUTF("");
    std::vector<llama_token> tokens((size_t) required);
    int count = llama_tokenize(vocab, prompt.data(), (int)prompt.size(), tokens.data(), required, true, true);
    if (count <= 0) return env->NewStringUTF("");
    auto cp = llama_context_default_params();
    cp.n_ctx = nCtx;
    cp.n_batch = 256;
    cp.n_threads = 4;
    cp.n_threads_batch = 4;
    llama_context *ctx = llama_init_from_model(g_model, cp);
    if (!ctx) return env->NewStringUTF("");
    auto sp = llama_sampler_chain_default_params();
    llama_sampler *sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f,1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.75f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
    bool ok = true;
    for (int p = 0; p < count; p += 256) {
        int n = std::min(256, count - p);
        llama_batch batch = llama_batch_get_one(tokens.data() + p,n);
        if (llama_decode(ctx,batch) != 0) { ok = false; break; }
    }
    std::string result;
    if (ok) for (int i=0;i<maxOut;++i) {
        llama_token tok = llama_sampler_sample(sampler,ctx,-1);
        if (llama_vocab_is_eog(vocab,tok)) break;
        char piece[512];
        int n = llama_token_to_piece(vocab,tok,piece,sizeof(piece),0,false);
        if (n > 0) result.append(piece,(size_t)n);
        llama_batch batch = llama_batch_get_one(&tok,1);
        if (llama_decode(ctx,batch) != 0) break;
    }
    llama_sampler_free(sampler);
    llama_free(ctx);
    // JNI's NewStringUTF accepts modified UTF-8; avoid embedded NUL or mid-token truncations.
    result.erase(std::remove(result.begin(),result.end(),'\0'),result.end());
    return env->NewStringUTF(result.c_str());
}
