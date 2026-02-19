#include "wake/wake_listener.hpp"

#include <iostream>

int main() {

    std::cerr << "Initializing WakeListener...\n";
    std::cerr.flush();
    WakeListener wake("127.0.0.1", 3939, [&](const std::string& msg) {
        std::cout << "[wake] " << msg << std::endl;

    });
    wake.start();

    std::string text;
    std::cout << "\nMain running (press Enter to quit): ";
    std::getline(std::cin, text);

    wake.stop();
    return 0;
}
