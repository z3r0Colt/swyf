#include <jni.h>
#include <string>
#include "dialtone-engine.h"

static std::string from_jstring(JNIEnv *env, jstring s) {
    if (!s) return {};
    const char *p = env->GetStringUTFChars(s, nullptr);
    if (!p) return {};
    std::string value(p);
    env->ReleaseStringUTFChars(s, p);
    return value;
}

// Raw UTF-8 bytes. NewStringUTF only accepts modified UTF-8 and aborts on emoji, so Java decodes instead.
static jbyteArray to_bytes(JNIEnv *env, const std::string &s) {
    jbyteArray bytes = env->NewByteArray((jsize) s.size());
    if (bytes && !s.empty()) env->SetByteArrayRegion(bytes, 0, (jsize) s.size(), reinterpret_cast<const jbyte *>(s.data()));
    return bytes;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_gentleking_dialtone_NativeLlama_load(JNIEnv *env, jclass, jstring jpath, jint threads, jstring jlib_dir) {
    return dialtone::load_model(from_jstring(env, jpath), (int) threads, from_jstring(env, jlib_dir)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_org_gentleking_dialtone_NativeLlama_generate(JNIEnv *env, jclass, jstring jprompt, jint max_tokens,
                                                  jboolean force_tag, jobject listener) {
    dialtone::OnText on_text;
    jmethodID on_text_id = nullptr;
    if (listener) {
        jclass cls = env->GetObjectClass(listener);
        on_text_id = env->GetMethodID(cls, "onText", "([B)V");
        env->DeleteLocalRef(cls);
    }
    if (on_text_id) {
        on_text = [env, listener, on_text_id](const std::string &text) {
            jbyteArray bytes = to_bytes(env, text);
            if (!bytes) return;
            env->CallVoidMethod(listener, on_text_id, bytes);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(bytes);
        };
    }
    std::string out = dialtone::generate(from_jstring(env, jprompt), (int) max_tokens, force_tag == JNI_TRUE, on_text);
    return to_bytes(env, out);
}
