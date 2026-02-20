#include "headers.hpp"

#include <iostream>

int main() {

    // STT model init
    WhisperSTT stt("models/whisper/ggml-base.en-q5_1.bin");

    // WakeHandler init
    WakeHandler wake("127.0.0.1", 3939,
        [&](const std::string& msg, const std::string& senderIP, uint16_t senderPort) {
        bool expected = false;

        if (!stt.session_active_.compare_exchange_strong(expected, true)) {
            std::cout << "[Wake Handler] [WARN] Callback triggered with STT session still active. Backend may not own audio input in current conversation." << std::endl;
            return;
        }

        std::thread([&]{
            try {
                // TODO: add STT -> LLM loop until notifying wake_word.py to take audio input back
                run_live_stt(stt);
            } catch (const std::exception& e) {
                std::cerr << "[Whisper STT] [ERROR] " << e.what() << std::endl;
            }
            stt.session_active_.store(false);
        }).detach();
    });

    wake.start();

    std::string text;
    std::cout << "\nBackend running... Press enter to quit." << std::endl;
    std::getline(std::cin, text);

    wake.stop();
    return 0;
}
