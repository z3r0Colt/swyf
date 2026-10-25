package org.gentleking.dialtone;

final class NativeLlama {
    static { System.loadLibrary("dialtone-llama"); }
    static native boolean load(String path);
    static native String generate(String prompt, int maxTokens);
    private NativeLlama() {}
}
