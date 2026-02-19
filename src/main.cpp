#include "wake/wake_listener.hpp"

#include <iostream>

int main() {

    // WakeListener init
    WakeListener wake("127.0.0.1", 3939, [&](const std::string& msg) {
        std::cout << "[wake] " << msg << std::endl;



    });
    wake.start();


    std::string text;
    std::cout << "\nBackend running... Press enter to quit." << std::endl;
    std::getline(std::cin, text);


    wake.stop();
    return 0;
}
