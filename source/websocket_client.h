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
#include <winhttp.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

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
    bool m_secure; // wss/https

    // WinHTTP handles for WSS mode
    HINTERNET m_hSession = nullptr;
    HINTERNET m_hConnect = nullptr;
    HINTERNET m_hRequest = nullptr;
    HINTERNET m_hWebSocket = nullptr;
    
    bool parse_url(const std::string& url);
    bool resolve_and_connect();
    bool perform_handshake();
    std::string create_handshake_request();
    std::string base64_encode(const std::string& input);
    void receive_loop();
    std::string websocket_frame_decode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> websocket_frame_encode(const std::string& message);

    // WSS (WinHTTP) path
    bool connect_wss(const std::string& url);
    void receive_loop_wss();

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
