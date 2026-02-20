#include "wake/wake_listener.hpp"

#include <iostream>

int run_live_stt(const std::string& modelPath);

int main() {

    // WakeListener init
    WakeListener wake("127.0.0.1", 3939, [&](const std::string& msg) {
         std::cout << "[wake] " << msg << std::endl;



    });
    wake.start();

    try{ 
        run_live_stt("models/whisper/ggml-base.en-q5_1.bin");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFATAL: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\nFATAl: unknown exception\n";
        return 1;
    }


    std::string text;
    std::cout << "\nBackend running... Press enter to quit." << std::endl;
    std::getline(std::cin, text);


    wake.stop();
    return 0;
}
