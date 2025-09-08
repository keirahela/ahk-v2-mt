#pragma once

#include "stdafx.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <memory>
#include <windows.h>
#include <condition_variable>

class SimpleThreading {
public:
    class ThreadInterpreter;

private:
    static std::atomic<int> s_threadCount;
    static std::mutex s_globalMutex;
    // Map of interpreters per thread id
    static std::unordered_map<DWORD, std::unique_ptr<ThreadInterpreter>> s_interpreters;
    
public:
    static void IncrementThreadCount();
    static void DecrementThreadCount();
    static int GetThreadCount();
    
    // Thread creation/management
    static DWORD CreateThread(const std::string& script);
    static bool DestroyThread(DWORD threadId);
    static bool PauseThread(DWORD threadId);
    static bool ResumeThread(DWORD threadId);
    static bool WaitForThread(DWORD threadId, int timeoutMs = -1);
    
    // Shared variable store
    static bool SetGlobalVar(const std::string& name, const std::string& value);
    static std::string GetGlobalVar(const std::string& name);
    static bool HasGlobalVar(const std::string& name);
    
private:
    static std::unordered_map<DWORD, std::unique_ptr<std::thread>> s_threads;
    static std::unordered_map<std::string, std::string> s_globalVars;
    static std::atomic<DWORD> s_nextThreadId;

    // Reserved for potential out-of-process workers
    static std::unordered_map<DWORD, HANDLE> s_processes;

    // Cooperative shutdown flags
    static std::unordered_map<DWORD, std::atomic<bool>> s_stopFlags;
};

// Global instance
extern SimpleThreading g_SimpleThreading;
