#include "stdafx.h"
#include "simple_threading_api.h"
#include "simple_threading.h"
#include "globaldata.h"
#include "script_func_impl.h"

BIF_DECL(BIF_ThreadCreate)
{
    // Get script string from parameter using proper AutoHotkey parameter handling
    _f_param_string(script_str, 0);
    
    // Convert LPTSTR to std::string
    std::string script;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, script_str, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        script.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, script_str, -1, &script[0], len, NULL, NULL);
    }
#else
    script = script_str;
#endif
    
    // Create thread
    DWORD threadId = SimpleThreading::CreateThread(script);
    
    if (threadId == 0) {
        _f_throw(_T("Failed to create thread."));
    }
    
    // Return thread ID
    _f_return_i(threadId);
}

BIF_DECL(BIF_ThreadDestroy)
{
    if (aParamCount < 1) {
        _f_throw(_T("ThreadDestroy requires one parameter (thread ID)."));
    }
    
    DWORD threadId = static_cast<DWORD>(aParam[0]->value_int64);
    bool success = SimpleThreading::DestroyThread(threadId);
    
    _f_return_i(success ? 1 : 0);
}

BIF_DECL(BIF_ThreadCount)
{
    int count = SimpleThreading::GetThreadCount();
    _f_return_i(count);
}

BIF_DECL(BIF_ThreadSetVar)
{
    // Get variable name and value using proper AutoHotkey parameter handling
    _f_param_string(varName_str, 0);
    _f_param_string(varValue_str, 1);
    
    // Convert LPTSTR to std::string
    std::string varName, varValue;
#ifdef UNICODE
    int len1 = WideCharToMultiByte(CP_UTF8, 0, varName_str, -1, NULL, 0, NULL, NULL);
    if (len1 > 0) {
        varName.resize(len1 - 1);
        WideCharToMultiByte(CP_UTF8, 0, varName_str, -1, &varName[0], len1, NULL, NULL);
    }
    int len2 = WideCharToMultiByte(CP_UTF8, 0, varValue_str, -1, NULL, 0, NULL, NULL);
    if (len2 > 0) {
        varValue.resize(len2 - 1);
        WideCharToMultiByte(CP_UTF8, 0, varValue_str, -1, &varValue[0], len2, NULL, NULL);
    }
#else
    varName = varName_str;
    varValue = varValue_str;
#endif
    
    // Set global variable
    bool success = SimpleThreading::SetGlobalVar(varName, varValue);
    _f_return_i(success ? 1 : 0);
}

BIF_DECL(BIF_ThreadGetVar)
{
    // Get variable name using proper AutoHotkey parameter handling
    _f_param_string(varName_str, 0);
    
    // Convert LPTSTR to std::string
    std::string varName;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, varName_str, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        varName.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, varName_str, -1, &varName[0], len, NULL, NULL);
    }
#else
    varName = varName_str;
#endif
    
    // Get global variable
    std::string value = SimpleThreading::GetGlobalVar(varName);
    
#ifdef UNICODE
    // Convert std::string to wide string
    int valueLen = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, NULL, 0);
    if (valueLen > 0) {
        wchar_t* wideStr = new wchar_t[valueLen];
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wideStr, valueLen);
        _f_set_retval_p(wideStr, valueLen - 1);
        delete[] wideStr;
    } else {
        _f_set_retval_p(_T(""), 0);
    }
#else
    _f_set_retval_p(value.c_str(), value.length());
#endif
}
