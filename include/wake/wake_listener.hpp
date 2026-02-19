#ifndef WAKE_LISTENER_HPP
#define WAKE_LISTENER_HPP

#include <functional>
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
  #include <winsock2.h>
  using socket_t = SOCKET;
  static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
  using socket_t = int;
  static constexpr socket_t kInvalidSocket = -1;
#endif

class WakeListener {
public:
    using CallBack = std::function<void(const std::string& payload)>;    

    WakeListener(std::string bind_ip, int port, CallBack callback_function);
    ~WakeListener();

    void start();
    void stop();

private:
    void run();
    
    std::string bind_ip_;
    int port_;
    CallBack callback_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    socket_t sock_{kInvalidSocket};
};

#endif
