#include "stdafx.h"
#include "simple_threading.h"
#include <sstream>
#include "websocket_client.h"

// Static member definitions
std::atomic<int> SimpleThreading::s_threadCount(0);
std::mutex SimpleThreading::s_globalMutex;
std::unordered_map<DWORD, std::unique_ptr<std::thread>> SimpleThreading::s_threads;
std::unordered_map<std::string, std::string> SimpleThreading::s_globalVars;
std::atomic<DWORD> SimpleThreading::s_nextThreadId(1);
std::unordered_map<DWORD, std::unique_ptr<SimpleThreading::ThreadInterpreter>> SimpleThreading::s_interpreters;
std::unordered_map<DWORD, HANDLE> SimpleThreading::s_processes;
std::unordered_map<DWORD, std::atomic<bool>> SimpleThreading::s_stopFlags;

class SimpleThreading::ThreadInterpreter {
public:
    explicit ThreadInterpreter(DWORD id) : m_id(id) {}
    void run(const std::string &script)
    {
        // Per-thread locals for basic expression/eval
        std::unordered_map<std::string, long long> locals;

        auto trim = [](std::string &s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) { s.clear(); return; }
            s = s.substr(a, b - a + 1);
        };

        auto stripQuotes = [](std::string s) -> std::string {
            if (s.size() >= 2 && ((s.front()== '"' && s.back()== '"') || (s.front()== '\'' && s.back()== '\'')))
                return s.substr(1, s.size()-2);
            return s;
        };

        auto parseInt = [](const std::string &s, long long &out) -> bool {
            try { out = std::stoll(s); return true; } catch (...) { return false; }
        };

        auto sleepMs = [](int ms) {
            if (ms < 0) ms = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        };

        auto setErrorAndStop = [&](const std::string &msg) {
            SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_error", msg);
        };

        auto evalCond = [&](std::string expr) -> bool {
            trim(expr);
            // support: var <op> number
            const char* ops[] = {"<=",">=","==","!=","<",">"};
            int which = -1; size_t pos = std::string::npos;
            for (int i=0;i<6;i++){ size_t p = expr.find(ops[i]); if (p!=std::string::npos){ which=i; pos=p; break; } }
            if (which == -1) { long long v; if (parseInt(expr, v)) return v!=0; return false; }
            std::string lhs = expr.substr(0,pos), rhs = expr.substr(pos + std::string(ops[which]).size());
            trim(lhs); trim(rhs);
            long long l=0,r=0; auto itL = locals.find(lhs); if (itL!=locals.end()) l=itL->second; else parseInt(lhs,l);
            parseInt(rhs,r);
            switch(which){
                case 0: return l<=r;
                case 1: return l>=r;
                case 2: return l==r;
                case 3: return l!=r;
                case 4: return l< r;
                case 5: return l> r;
            }
            return false;
        };

        // Load script into lines
        std::vector<std::string> lines;
        {
            std::istringstream ss(script);
            std::string l; while (std::getline(ss,l)) { trim(l); if (!l.empty() && l[0] != ';') lines.push_back(l); }
        }

        // Execute with minimal control flow support
        std::vector<size_t> loopStack;           // positions of while
        std::vector<bool>  execStack;            // whether current if-block is executing
        size_t pc = 0; bool exec = true;
        while (pc < lines.size()) {
            auto itStop = SimpleThreading::s_stopFlags.find(m_id);
            if (itStop != SimpleThreading::s_stopFlags.end() && itStop->second.load()) break;

            std::string line = lines[pc];
            SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_line", std::to_string((int)pc+1));

            // while ... / end
            if (line.rfind("while ",0) == 0) {
                bool cond = exec ? evalCond(line.substr(6)) : false;
                if (exec) {
                    if (cond) { loopStack.push_back(pc); execStack.push_back(exec); pc++; continue; }
                    // skip to matching end
                    int depth=1; size_t q=pc+1; while (q<lines.size() && depth>0){
                        if (lines[q].rfind("while ",0)==0) depth++;
                        else if (lines[q]=="end") depth--; else if (lines[q].rfind("if ",0)==0) { /* skip but track ends */ }
                        q++; }
                    pc = q; continue;
                } else { pc++; continue; }
            }
            if (line == "end") {
                if (!loopStack.empty()) { size_t start = loopStack.back(); bool parentExec = execStack.back(); loopStack.pop_back(); execStack.pop_back();
                    // re-eval condition
                    bool cond = parentExec ? evalCond(lines[start].substr(6)) : false;
                    if (parentExec && cond) { pc = start+1; continue; }
                    exec = parentExec; pc++; continue; }
                exec = true; pc++; continue;
            }

            // if / else
            if (line.rfind("if ",0) == 0) {
                bool cond = exec ? evalCond(line.substr(3)) : false;
                execStack.push_back(exec);
                exec = exec && cond; pc++; continue;
            }
            if (line == "else") {
                if (execStack.empty()) { setErrorAndStop("else without if"); break; }
                bool parent = execStack.back();
                // flip exec only within this if-else
                exec = parent && !exec; pc++; continue;
            }

            // Skip when current block is inactive
            if (!exec) { pc++; continue; }

            // Supported commands
            if (line.rfind("WebSocketConnect(", 0) == 0 && line.back() == ')') {
                std::string arg = line.substr(17, line.size()-18); trim(arg);
                std::string url = stripQuotes(arg);
                bool ok = g_WebSocketClient->connect(url);
                SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_ws_status", ok ? "connected" : "failed");
                pc++; continue;
            }
            if (line.rfind("WebSocketSend(", 0) == 0 && line.back() == ')') {
                std::string arg = line.substr(14, line.size()-15); trim(arg);
                std::string msg = stripQuotes(arg);
                g_WebSocketClient->send_message(msg);
                SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_ws_last_sent", msg);
                pc++; continue;
            }
            if (line == "WebSocketReceive()") {
                std::string msg = g_WebSocketClient->receive_message();
                if (!msg.empty()) SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_ws_last_received", msg);
                pc++; continue;
            }
            if (line == "WebSocketDisconnect()") {
                g_WebSocketClient->disconnect();
                SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_ws_status", "disconnected");
                pc++; continue;
            }
            if (line.rfind("ThreadSetVar(", 0) == 0 && line.back() == ')') {
                std::string args = line.substr(13, line.size() - 14); size_t comma = args.find(',');
                if (comma != std::string::npos) { std::string k = args.substr(0, comma); std::string v = args.substr(comma + 1); trim(k); trim(v); k=stripQuotes(k); v=stripQuotes(v); SimpleThreading::SetGlobalVar(k, v); }
                else { setErrorAndStop("ThreadSetVar requires 2 args"); break; }
                pc++; continue;
            }
            if (line.rfind("ThreadGetVar(", 0) == 0 && line.back() == ')') {
                std::string arg = line.substr(13, line.size() - 14); trim(arg); arg=stripQuotes(arg);
                std::string val = SimpleThreading::GetGlobalVar(arg);
                SimpleThreading::SetGlobalVar("thread_" + std::to_string(m_id) + "_last_get", val);
                pc++; continue;
            }
            if (line.rfind("Sleep(", 0) == 0 && line.back() == ')') {
                std::string arg = line.substr(6, line.size() - 7); trim(arg); long long ms; if (parseInt(arg, ms)) sleepMs((int)ms); else sleepMs(1);
                pc++; continue;
            }
            if (line.rfind("SetVar(", 0) == 0 && line.back() == ')') {
                std::string args = line.substr(7, line.size() - 8); size_t comma = args.find(',');
                if (comma != std::string::npos) { std::string k = args.substr(0, comma); std::string v = args.substr(comma + 1); trim(k); trim(v); k=stripQuotes(k); v=stripQuotes(v); SimpleThreading::SetGlobalVar(k, v); }
                pc++; continue;
            }
            if (line.size() >= 3 && line.substr(line.size()-2) == "++") {
                std::string var = line.substr(0, line.size()-2); trim(var); long long cur = 0; auto itL = locals.find(var); if (itL != locals.end()) cur = itL->second; locals[var] = cur + 1; pc++; continue;
            }
            size_t asn = line.find(":=");
            if (asn != std::string::npos) {
                std::string var = line.substr(0, asn), rhs = line.substr(asn + 2); trim(var); trim(rhs); long long val; if (parseInt(rhs, val)) locals[var] = val; pc++; continue;
            }

            if (!line.empty() && line.find('(') != std::string::npos && line.back() == ')') { setErrorAndStop("Unsupported function in thread: " + line); break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); pc++;
        }

        // Lightweight per-thread message loop with a simple heartbeat
        DWORD lastTick = GetTickCount();
        while (true) {
            auto it = SimpleThreading::s_stopFlags.find(m_id);
            if (it != SimpleThreading::s_stopFlags.end() && it->second.load()) break;
            MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            DWORD now = GetTickCount(); if (now - lastTick >= 10) { lastTick = now; std::string beatKey = std::string("thread_") + std::to_string(m_id) + "_heartbeat"; std::string beat = SimpleThreading::GetGlobalVar(beatKey); int beatVal = beat.empty() ? 0 : atoi(beat.c_str()); SimpleThreading::SetGlobalVar(beatKey, std::to_string(beatVal + 1)); }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
private:
    DWORD m_id;
};

void SimpleThreading::IncrementThreadCount() { s_threadCount++; }
void SimpleThreading::DecrementThreadCount() { s_threadCount--; }
int SimpleThreading::GetThreadCount() { return s_threadCount.load(); }

DWORD SimpleThreading::CreateThread(const std::string& script) {
    std::lock_guard<std::mutex> lock(s_globalMutex);
    DWORD threadId = s_nextThreadId++;
    auto interpreter = std::make_unique<ThreadInterpreter>(threadId);
    s_stopFlags[threadId] = false;
    auto thread = std::make_unique<std::thread>([script, threadId, intr = interpreter.get()]() {
        SimpleThreading::SetGlobalVar("thread_" + std::to_string(threadId) + "_status", "running");
        intr->run(script);
        SimpleThreading::SetGlobalVar("thread_" + std::to_string(threadId) + "_status", "completed");
    });
    s_threads[threadId] = std::move(thread);
    s_interpreters[threadId] = std::move(interpreter);
    return threadId;
}

bool SimpleThreading::DestroyThread(DWORD threadId) {
    std::unique_ptr<std::thread> toJoin;
    {
        std::lock_guard<std::mutex> lock(s_globalMutex);
        auto itFlag = s_stopFlags.find(threadId); if (itFlag != s_stopFlags.end()) itFlag->second.store(true);
        auto it = s_threads.find(threadId); if (it != s_threads.end()) { toJoin = std::move(it->second); s_threads.erase(it); }
    }
    if (toJoin) {
        if (toJoin->joinable()) toJoin->join();
        std::lock_guard<std::mutex> lock(s_globalMutex);
        auto itIntr = s_interpreters.find(threadId); if (itIntr != s_interpreters.end()) s_interpreters.erase(itIntr);
        s_stopFlags.erase(threadId);
        return true;
    }
    return false;
}

bool SimpleThreading::PauseThread(DWORD) { return true; }
bool SimpleThreading::ResumeThread(DWORD) { return true; }

bool SimpleThreading::WaitForThread(DWORD threadId, int) {
    std::unique_ptr<std::thread> toJoin;
    {
        std::lock_guard<std::mutex> lock(s_globalMutex);
        auto it = s_threads.find(threadId);
        if (it != s_threads.end()) { toJoin = std::make_unique<std::thread>(); toJoin.swap(it->second); s_threads.erase(it); }
    }
    if (toJoin) { if (toJoin->joinable()) toJoin->join(); return true; }
    return false;
}

bool SimpleThreading::SetGlobalVar(const std::string& name, const std::string& value) { std::lock_guard<std::mutex> lock(s_globalMutex); s_globalVars[name] = value; return true; }
std::string SimpleThreading::GetGlobalVar(const std::string& name) { std::lock_guard<std::mutex> lock(s_globalMutex); auto it = s_globalVars.find(name); return (it != s_globalVars.end()) ? it->second : ""; }
bool SimpleThreading::HasGlobalVar(const std::string& name) { std::lock_guard<std::mutex> lock(s_globalMutex); return s_globalVars.find(name) != s_globalVars.end(); }

// Global instance is defined in globaldata.cpp
