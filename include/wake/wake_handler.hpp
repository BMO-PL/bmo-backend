#ifndef WAKE_HANDLER_HPP
#define WAKE_HANDLER_HPP

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <mutex>

#ifdef _WIN32
  #include <winsock2.h>
  using socket_t = SOCKET;
  static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
  using socket_t = int;
  static constexpr socket_t kInvalidSocket = -1;
#endif

class WakeHandler {
public:
    using CallBack = std::function<void(const std::string& msg,
                                        const std::string& senderIp,
                                        uint16_t senderPort)>;    

    WakeHandler(std::string bind_ip, int port, CallBack callback_function);
    ~WakeHandler();

    void start();
    void stop();

    bool sendTo(const std::string& ip, uint16_t port, const std::string& payload);
    bool sendToActive(const std::string& payload);

    bool sendSessionDone();

private:
    void run();

    void setActiveClient(const std::string& ip, uint16_t port);
    
    std::string bind_ip_;
    int port_;
    CallBack callback_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    socket_t sock_{kInvalidSocket};

    std::mutex client_mutex_;
    std::string active_ip_{"127.0.0.1"};
    uint16_t active_port_{0};
    bool has_active_client_{false};
};

#endif
