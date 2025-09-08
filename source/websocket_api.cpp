#include "stdafx.h"
#include "websocket_api.h"
#include "simple_threading.h"
#include "globaldata.h"
#include "script_func_impl.h"
#include "websocket_client.h"
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")

BIF_DECL(BIF_WebSocketConnect)
{
    // Get URL parameter using proper AutoHotkey parameter handling
    _f_param_string(url_str, 0);
    
    // Convert LPTSTR to std::string
    std::string url;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, url_str, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        url.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, url_str, -1, &url[0], len, NULL, NULL);
    }
#else
    url = url_str;
#endif
    
    // Connect to the real WebSocket server
    bool success = g_WebSocketClient->connect(url);
    
    if (success) {
        SimpleThreading::SetGlobalVar("websocket_url", url);
        SimpleThreading::SetGlobalVar("websocket_status", "connected");
        _f_return_i(1); // Return success
    } else {
        SimpleThreading::SetGlobalVar("websocket_status", "failed");
        _f_return_i(0); // Return failure
    }
}

BIF_DECL(BIF_WebSocketSend)
{
    // Get message parameter using proper AutoHotkey parameter handling
    _f_param_string(message_str, 0);
    
    // Convert LPTSTR to std::string
    std::string message;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, message_str, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        message.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, message_str, -1, &message[0], len, NULL, NULL);
    }
#else
    message = message_str;
#endif
    
    // Send the message through the real WebSocket connection
    bool success = g_WebSocketClient->send_message(message);
    
    if (success) {
        SimpleThreading::SetGlobalVar("websocket_last_sent", message);
        _f_return_i(1); // Return success
    } else {
        _f_return_i(0); // Return failure
    }
}

BIF_DECL(BIF_WebSocketReceive)
{
    // Get the last received message from the real WebSocket connection
    std::string message = g_WebSocketClient->receive_message();
    
    // Convert std::string to LPTSTR
    if (message.empty()) {
        _f_set_retval_p(_T(""), 0);
    } else {
#ifdef UNICODE
        int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
        if (len > 0) {
            wchar_t* result = new wchar_t[len];
            MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, result, len);
            _f_set_retval_p(result, len - 1);
            delete[] result;
        } else {
            _f_set_retval_p(_T(""), 0);
        }
#else
        _f_set_retval_p(message.c_str(), message.length());
#endif
    }
}

BIF_DECL(BIF_WebSocketDisconnect)
{
    // Disconnect from the real WebSocket server
    g_WebSocketClient->disconnect();
    SimpleThreading::SetGlobalVar("websocket_status", "disconnected");
    _f_return_i(1);
}

BIF_DECL(BIF_HttpRequest)
{
    // Get URL and method parameters using proper AutoHotkey parameter handling
    _f_param_string(url_str, 0);
    _f_param_string(method_str, 1);
    
    // Convert LPTSTR to std::string
    std::string url, method;
#ifdef UNICODE
    int urlLen = WideCharToMultiByte(CP_UTF8, 0, url_str, -1, NULL, 0, NULL, NULL);
    if (urlLen > 0) {
        url.resize(urlLen - 1);
        WideCharToMultiByte(CP_UTF8, 0, url_str, -1, &url[0], urlLen, NULL, NULL);
    }
    int methodLen = WideCharToMultiByte(CP_UTF8, 0, method_str, -1, NULL, 0, NULL, NULL);
    if (methodLen > 0) {
        method.resize(methodLen - 1);
        WideCharToMultiByte(CP_UTF8, 0, method_str, -1, &method[0], methodLen, NULL, NULL);
    }
#else
    url = url_str;
    method = method_str;
#endif
    
    // For now, just simulate a successful HTTP request
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"success\",\"message\":\"Request processed\"}";
    SimpleThreading::SetGlobalVar("http_last_response", response);
    
    // Convert response to LPTSTR
#ifdef UNICODE
    int len = MultiByteToWideChar(CP_UTF8, 0, response.c_str(), -1, NULL, 0);
    if (len > 0) {
        wchar_t* result = new wchar_t[len];
        MultiByteToWideChar(CP_UTF8, 0, response.c_str(), -1, result, len);
        _f_set_retval_p(result, len - 1);
        delete[] result;
    } else {
        _f_set_retval_p(_T(""), 0);
    }
#else
    _f_set_retval_p(response.c_str(), response.length());
#endif
}
