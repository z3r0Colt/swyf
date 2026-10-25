// Desktop test harness: dialtone-chat-test <model.gguf> then one prompt per stdin record, records end with a line "<<<END>>>".
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include "dialtone-engine.h"

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s model.gguf < prompts\n", argv[0]); return 2; }
    if (!dialtone::load_model(argv[1], (int) std::thread::hardware_concurrency())) { std::fprintf(stderr, "load failed\n"); return 1; }
    std::string line, prompt;
    while (std::getline(std::cin, line)) {
        if (line != "<<<END>>>") { prompt += line + "\n"; continue; }
        std::string out = dialtone::generate(prompt, 160, true);
        dialtone::Stats st = dialtone::last_stats();
        std::fprintf(stderr, "prompt %d tok (%d reused) %.0f ms, reply %d tok %.0f ms\n", st.prompt_tokens, st.reused_tokens, st.prompt_ms, st.gen_tokens, st.gen_ms);
        std::cout << out << "\n<<<END>>>" << std::endl;
        prompt.clear();
    }
    return 0;
}
