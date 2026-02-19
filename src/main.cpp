#include "wake/wake_listener.hpp"

#include <iostream>

int main(int argc, char** argv) {

    WakeListener wake("127.0.0.1", 3939, [&](const std::string& msg) {
        std::cout << "[wake] " << msg << std::endl;

    });
    wake.start();

    std::string text;
    std::cout << "\nMain running: ";
    std::cin >> text;

    wake.stop();
    return 0;
}
