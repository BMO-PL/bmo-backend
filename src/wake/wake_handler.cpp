#include "wake/wake_handler.hpp"

#include <cstring>
#include <iostream>
#include <utility>
#include <chrono>

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
WakeHandler::WakeHandler(std::string bind_ip, int port, CallBack callback_function)
    : bind_ip_(std::move(bind_ip)), port_(port), callback_(std::move(callback_function)) {}

// Destructor
WakeHandler::~WakeHandler() { stop(); }

// Starts the communication thread in the C++ backend
void WakeHandler::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&WakeHandler::run, this);
}

// Stops the communication thread
void WakeHandler::stop() {
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

// Sets current active client the handler is communicating with
void WakeHandler::setActiveClient(const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    active_ip_ = ip;
    active_port_ = port;
    has_active_client_ = true;
}

// Sends a payload to an ip and port
bool WakeHandler::sendTo(const std::string& ip, uint16_t port, const std::string& payload) {
    if (sock_ == kInvalidSocket) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }

#ifdef _WIN32
    int n = ::sendto(sock_, payload.data(), (int)payload.size(), 0,
                     reinterpret_cast<sockaddr*>(&addr), (int)sizeof(addr));
    return n == (int)payload.size();
#else
    ssize_t n = ::sendto(sock_, payload.data(), payload.size(), 0,
                         reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return n == (ssize_t)payload.size();
#endif
}

// Sends a payload to the active client
bool WakeHandler::sendToActive(const std::string& payload) {
    std::string ip;
    uint16_t port = 0;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!has_active_client_) return false;
        ip = active_ip_;
        port = active_port_;
    }
    return sendTo(ip, port, payload);
}

// Lets wakeword script (current client) know the backend record session is done
bool WakeHandler::sendSessionDone() {
    const auto ts = (double)std::time(nullptr);
    std::string msg = std::string("{\"type\":\"session_done\",\"ts\":") + std::to_string(ts) + "}";
    return sendToActive(msg);
}

// Thread function that listens for a response from the wake_word.py script
void WakeHandler::run() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WakeHandler: WSAStartup failed\n";
        running_ = false;
        return;
    }
#endif

    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == kInvalidSocket) {
#ifdef _WIN32
        std::cerr << "WakeHandler socket() failed: " << WSAGetLastError() << "\n";
#else
        std::cerr << "WakeHandler socket() failed: " << std::strerror(errno) << "\n";
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
        std::cerr << "WakeHandler: invalid bind ip: " << bind_ip_ << "\n";
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
        std::cerr << "WakeHandler bind() failed: " << WSAGetLastError() << "\n";
#else
        std::cerr << "WakeHandler bind() failed: " << std::strerror(errno) << "\n";
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
        const int n = ::recvfrom(sock_, buff, (int)sizeof(buff) - 1, 0,
                                reinterpret_cast<sockaddr*>(&src), &slen);
#else
        socklen_t slen = sizeof(src);
        const ssize_t n = ::recvfrom(sock_, buff, sizeof(buff) - 1, 0,
                                     reinterpret_cast<sockaddr*>(&src), &slen);
#endif

        if (n <= 0) break;
        buff[n] = '\0';

        char ipstr[INET_ADDRSTRLEN]{};
        const char* ok = ::inet_ntop(AF_INET, &src.sin_addr, ipstr, sizeof(ipstr));
        std::string senderIp = ok ? std::string(ipstr) : std::string("127.0.0.1");
        uint16_t senderPort = ntohs(src.sin_port);

        setActiveClient(senderIp, senderPort);

        try {
            if (callback_) callback_(std::string(buff), senderIp, senderPort);
        } catch (...) {
            std::cerr << "WakeHandler callback threw\n";
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