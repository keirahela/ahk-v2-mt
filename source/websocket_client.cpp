#include "stdafx.h"
#include "websocket_client.h"
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>

WebSocketClient::WebSocketClient() : m_socket(INVALID_SOCKET), m_connected(false), m_running(false), m_port(0) {
    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

WebSocketClient::~WebSocketClient() {
    disconnect();
    WSACleanup();
}

bool WebSocketClient::parse_url(const std::string& url) {
    // Accept forms:
    //   ws://host[:port][/path]
    //   host[:port][/path]
    //   barehost (defaults to port 80 and path "/")

    std::string u = url;
    m_port = 80;
    m_path = "/";

    if (u.rfind("ws://", 0) == 0) {
        u = u.substr(5);
    }

    // Split host[:port] and path
    size_t slash = u.find('/');
    std::string host_port = slash == std::string::npos ? u : u.substr(0, slash);
    if (slash != std::string::npos) m_path = u.substr(slash);

    // Split host and optional port
    size_t colon = host_port.rfind(':');
    if (colon != std::string::npos) {
        m_host = host_port.substr(0, colon);
        try { m_port = std::stoi(host_port.substr(colon + 1)); } catch (...) { m_port = 80; }
    } else {
        m_host = host_port;
    }

    // Leave hostname as-is for DNS; allow localhost mapping to 127.0.0.1 via DNS path below.
    return !m_host.empty();
}

bool WebSocketClient::resolve_and_connect() {
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = nullptr;
    std::string portstr = std::to_string(m_port);
    if (getaddrinfo(m_host.c_str(), portstr.c_str(), &hints, &result) != 0) {
        return false;
    }

    SOCKET s = INVALID_SOCKET;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (::connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) {
            m_socket = s;
            freeaddrinfo(result);
            return true;
        }
        closesocket(s);
    }
    freeaddrinfo(result);
    return false;
}

std::string WebSocketClient::base64_encode(const std::string& input) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

std::string WebSocketClient::create_handshake_request() {
    // Random Sec-WebSocket-Key
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string key;
    for (int i = 0; i < 16; ++i) {
        key += static_cast<char>(dis(gen));
    }
    
    std::string encoded_key = base64_encode(key);
    
    std::ostringstream request;
    request << "GET " << m_path << " HTTP/1.1\r\n";
    request << "Host: " << m_host << ":" << m_port << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << encoded_key << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    request << "\r\n";
    
    return request.str();
}

bool WebSocketClient::perform_handshake() {
    std::string request = create_handshake_request();
    
    // Send handshake request
    int result = send(m_socket, request.c_str(), static_cast<int>(request.length()), 0);
    if (result == SOCKET_ERROR) {
        return false;
    }
    
    // Receive handshake response
    char buffer[1024];
    result = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    if (result == SOCKET_ERROR) {
        return false;
    }
    
    buffer[result] = '\0';
    std::string response(buffer);
    
    // Expect 101 Switching Protocols
    return response.find("101 Switching Protocols") != std::string::npos;
}

std::vector<uint8_t> WebSocketClient::websocket_frame_encode(const std::string& message) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN=1, opcode=1 (text)
    frame.push_back(0x81);
    
    // Payload length (MASK bit set)
    size_t payload_len = message.length();
    if (payload_len < 126) {
        frame.push_back(static_cast<uint8_t>(payload_len) | 0x80); // Set MASK bit
    } else if (payload_len < 65536) {
        frame.push_back(126 | 0x80); // Set MASK bit
        frame.push_back((payload_len >> 8) & 0xFF);
        frame.push_back(payload_len & 0xFF);
    } else {
        frame.push_back(127 | 0x80); // Set MASK bit
        for (int i = 7; i >= 0; --i) {
            frame.push_back((payload_len >> (i * 8)) & 0xFF);
        }
    }
    
    // Random masking key
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    uint8_t mask_key[4];
    for (int i = 0; i < 4; ++i) {
        mask_key[i] = static_cast<uint8_t>(dis(gen));
        frame.push_back(mask_key[i]);
    }
    
    // Masked payload
    for (size_t i = 0; i < message.length(); ++i) {
        uint8_t masked_byte = static_cast<uint8_t>(message[i]) ^ mask_key[i % 4];
        frame.push_back(masked_byte);
    }
    
    return frame;
}

std::string WebSocketClient::websocket_frame_decode(const std::vector<uint8_t>& data) {
    if (data.size() < 2) {
        return "";
    }
    
    bool fin = (data[0] & 0x80) != 0;
    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint8_t payload_len = data[1] & 0x7F;
    
    size_t header_len = 2;
    size_t actual_payload_len = payload_len;
    
    if (payload_len == 126) {
        if (data.size() < 4) return "";
        actual_payload_len = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (data.size() < 10) return "";
        actual_payload_len = 0;
        for (int i = 2; i < 10; ++i) {
            actual_payload_len = (actual_payload_len << 8) | data[i];
        }
        header_len = 10;
    }
    
    if (masked) {
        header_len += 4; // Masking key
    }
    
    if (data.size() < header_len + actual_payload_len) {
        return "";
    }
    
    std::string payload;
    payload.reserve(actual_payload_len);
    
    for (size_t i = 0; i < actual_payload_len; ++i) {
        uint8_t byte = data[header_len + i];
        if (masked) {
            byte ^= data[header_len - 4 + (i % 4)];
        }
        payload += static_cast<char>(byte);
    }
    
    return payload;
}

void WebSocketClient::receive_loop() {
    char buffer[4096];
    
    while (m_running && m_connected) {
        int result = recv(m_socket, buffer, sizeof(buffer), 0);
        if (result == SOCKET_ERROR || result == 0) {
            m_connected = false;
            break;
        }
        
        std::vector<uint8_t> data(buffer, buffer + result);
        std::string message = websocket_frame_decode(data);
        
        if (!message.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_messageQueue.push(message);
        }
    }
}

bool WebSocketClient::connect(const std::string& url) {
    if (m_connected || m_running) {
        std::cout << "WebSocket: Already connected or running" << std::endl;
        return false;
    }
    
    if (!parse_url(url)) {
        std::cout << "WebSocket: Failed to parse URL: " << url << std::endl;
        return false;
    }

    std::cout << "WebSocket: Connecting to " << m_host << ":" << m_port << m_path << std::endl;

    if (!resolve_and_connect()) {
        std::cout << "WebSocket: Failed to resolve/connect" << std::endl;
        return false;
    }
    
    std::cout << "WebSocket: Connected to server, performing handshake" << std::endl;
    
    // Perform WebSocket handshake
    if (!perform_handshake()) {
        std::cout << "WebSocket: Handshake failed" << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
    
    std::cout << "WebSocket: Handshake successful" << std::endl;
    
    m_connected = true;
    m_running = true;
    
    // Start receive thread
    m_thread = std::thread(&WebSocketClient::receive_loop, this);
    
    return true;
}

void WebSocketClient::disconnect() {
    if (!m_connected && !m_running) {
        return;
    }
    
    m_running = false;
    m_connected = false;
    
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool WebSocketClient::send_message(const std::string& message) {
    if (!m_connected) {
        return false;
    }
    
    std::vector<uint8_t> frame = websocket_frame_encode(message);
    int result = send(m_socket, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
    
    return result != SOCKET_ERROR;
}

std::string WebSocketClient::receive_message() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_messageQueue.empty()) {
        return "";
    }
    
    std::string message = m_messageQueue.front();
    m_messageQueue.pop();
    return message;
}

bool WebSocketClient::is_connected() const {
    return m_connected;
}