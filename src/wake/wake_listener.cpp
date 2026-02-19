#include "wake/wake_listener.hpp"

#include <cstring>
#include <iostream>
#include <utility>

#ifdef _WIN32
  #include <ws2tcpip.h>
#else
  #include <cerrno>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
#endif

#ifdef _WIN32
static void closesock(socket_t s) { ::closesocket(s); }
#else
static void closesock(socket_t s) { ::close(s); }
#endif

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

    if (sock_ != kInvalidSocket) {
#ifdef _WIN32
        ::shutdown(sock_, SD_BOTH);
#else
        ::shutdown(sock_, SHUT_RDWR);
#endif
        closesock(sock_);
        sock_ = kInvalidSocket;
    }

    if (thread_.joinable()) thread_.join();
}

// Thread function that listens for a response from the wake_word.py script
void WakeListener::run() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WakeListener: WSAStartup failed\n";
        running_ = false;
        return;
    }
#endif

    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == kInvalidSocket) {
#ifdef _WIN32
        std::cerr << "WakeListener socket() failed: " << WSAGetLastError() << "\n";
#else
        std::cerr << "WakeListener socket() failed: " << std::strerror(errno) << "\n";
#endif
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    int reuse = 1;
#ifdef _WIN32
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#else
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (::inet_pton(AF_INET, bind_ip_.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "WakeListener: invalid bind ip: " << bind_ip_ << "\n";
        closesock(sock_);
        sock_ = kInvalidSocket;
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
#ifdef _WIN32
        std::cerr << "WakeListener bind() failed: " << WSAGetLastError() << "\n";
#else
        std::cerr << "WakeListener bind() failed: " << std::strerror(errno) << "\n";
#endif
        closesock(sock_);
        sock_ = kInvalidSocket;
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    while (running_.load()) {
        char buff[2048];
        sockaddr_in src{};
#ifdef _WIN32
        int slen = sizeof(src);
#else
        socklen_t slen = sizeof(src);
#endif

#ifdef _WIN32
        const int n = ::recvfrom(sock_, buff, (int)sizeof(buff) - 1, 0,
                                reinterpret_cast<sockaddr*>(&src), &slen);
#else
        const ssize_t n = ::recvfrom(sock_, buff, sizeof(buff) - 1, 0,
                                     reinterpret_cast<sockaddr*>(&src), &slen);
#endif

        if (n <= 0) break;
        buff[n] = '\0';

        try {
            if (callback_) callback_(std::string(buff));
        } catch (...) {
            std::cerr << "WakeListener callback threw\n";
        }
    }

    if (sock_ != kInvalidSocket) {
        closesock(sock_);
        sock_ = kInvalidSocket;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}