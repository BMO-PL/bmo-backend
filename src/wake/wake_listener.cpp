#include "wake/wake_listener.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// Constructor
WakeListener::WakeListener(std::string bind_ip, int port, CallBack callback_function) 
    : bind_ip_(std::move(bind_ip)), port_(port), callback_(std::move(callback_function)) {}

// Destructor
WakeListener::~WakeListener() { stop(); }

// Starts the listening thread in the C++ backend
void WakeListener::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&WakeListener::run, this);
}

// Stops the listening thread
void WakeListener::stop() {
    if (!running_.exchange(false)) return;

    if (sock_ >= 0) {
        ::shutdown(sock_, SHUT_RDWR);
        ::close(sock_);
        sock_ = -1;
    }

    if (thread_.joinable()) thread_.join();
}

// Thread function that listens for a response from the wake_word.py script
void WakeListener::run() {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (sock_ < 0) {
        std::cerr << "WakeListener socket() failed: " << std::strerror(errno) << "\n";
        running_ = false;
        return;
    }

    int reuse = 1;
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port_);

    if (::inet_pton(AF_INET, bind_ip_.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "WakeListener: invalid bind ip: " << bind_ip_ << "\n";
        ::close(sock_);
        sock_ = -1;
        running_ = false;
        return;
    }

    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "WakeListener bind() failed: " << std::strerror(errno) << "\n";
        ::close(sock_);
        sock_ = -1;
        running_ = false;
        return;
    }

    while (running_.load()) {
        char buff[2048];
        sockaddr_in src{};
        socklen_t slen = sizeof(src);

        const ssize_t n = ::recvfrom(
            sock_, buff, sizeof(buff) - 1, 0,
            reinterpret_cast<sockaddr*>(&src), &slen
        );

        if (n <= 0) break;
        buff[n] = '\0';

        try {
            if (callback_) callback_(std::string(buff));
        } catch (...) {
            std::cerr << "WakeListener callback threw\n";
        }
    }
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
    }
}
