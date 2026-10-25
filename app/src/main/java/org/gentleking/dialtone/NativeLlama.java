package org.gentleking.dialtone;

final class NativeLlama {
    static { System.loadLibrary("dialtone-llama"); }

    /** Receives the UTF-8 reply text generated so far, after each token. */
    interface TextListener { void onText(byte[] utf8); }

    /** libDir holds the ggml CPU backend variants; the best one for this phone is picked at load. */
    static native boolean load(String path, int threads, String libDir);
    /** Returns the reply as UTF-8 bytes, empty on failure. */
    static native byte[] generate(String prompt, int maxTokens, boolean forceTag, TextListener listener);
    private NativeLlama() {}
}
