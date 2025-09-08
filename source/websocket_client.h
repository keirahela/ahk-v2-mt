#pragma once

#include "stdafx.h"
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <atomic>
#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class WebSocketClient {
private:
    SOCKET m_socket;
    std::thread m_thread;
    std::mutex m_mutex;
    std::queue<std::string> m_messageQueue;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_running;
    std::string m_url;
    std::string m_host;
    std::string m_path;
    int m_port;
    
    bool parse_url(const std::string& url);
    bool resolve_and_connect();
    bool perform_handshake();
    std::string create_handshake_request();
    std::string base64_encode(const std::string& input);
    void receive_loop();
    std::string websocket_frame_decode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> websocket_frame_encode(const std::string& message);

public:
    WebSocketClient();
    ~WebSocketClient();
    
    bool connect(const std::string& url);
    void disconnect();
    bool send_message(const std::string& message);
    std::string receive_message();
    bool is_connected() const;
};

// Global WebSocket client instance
extern std::unique_ptr<WebSocketClient> g_WebSocketClient;
