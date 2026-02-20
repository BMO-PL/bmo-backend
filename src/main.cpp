#include "headers.hpp"

#include <iostream>

int main() {

    // STT model init
    WhisperSTT stt("models/whisper/ggml-base.en-q5_1.bin");

    // WakeListener init
    WakeListener wake("127.0.0.1", 3939, [&](const std::string& msg) {
        std::cout << "[wake] " << msg << std::endl;

        if (stt.is_busy_.exchange(true)) return;

        std::thread([&]{
            try {
                run_live_stt(stt);
            } catch (const std::exception& e) {
                std::cerr << "\nSTT error: " << e.what() << "\n";
            }
            stt.is_busy_ = false;
        }).detach();

    });

    wake.start();

    std::string text;
    std::cout << "\nBackend running... Press enter to quit." << std::endl;
    std::getline(std::cin, text);

    wake.stop();
    return 0;
}
