#include "FileDialog.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>

namespace TopOpt {

// UTF-8 -> UTF-16
static std::wstring utf8ToWide(const char* str) {
    if (!str || !str[0]) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, ws.data(), len);
    return ws;
}

// UTF-16 -> UTF-8
static std::string wideToUtf8(const wchar_t* ws) {
    if (!ws || !ws[0]) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
    // Remove trailing null
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// Convert filter string (with embedded \0) to wide
static std::wstring filterToWide(const char* filter) {
    // Filter has embedded nulls, terminated by double null
    std::wstring result;
    const char* p = filter;
    while (*p) {
        std::wstring part = utf8ToWide(p);
        result += part;
        result += L'\0';
        p += strlen(p) + 1;
    }
    result += L'\0';
    return result;
}

std::string FileDialog::openFile(GLFWwindow* parentWindow, const char* title, const char* filter) {
    HWND hwnd = parentWindow ? glfwGetWin32Window(parentWindow) : nullptr;

    wchar_t filePath[MAX_PATH] = { 0 };
    std::wstring wTitle  = utf8ToWide(title);
    std::wstring wFilter = filterToWide(filter);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hwnd;
    ofn.lpstrFilter  = wFilter.c_str();
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = wTitle.c_str();
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return wideToUtf8(filePath);
    }
    return "";
}

std::string FileDialog::saveFile(GLFWwindow* parentWindow, const char* title,
                                  const char* filter, const char* defaultExt) {
    HWND hwnd = parentWindow ? glfwGetWin32Window(parentWindow) : nullptr;

    wchar_t filePath[MAX_PATH] = { 0 };
    std::wstring wTitle  = utf8ToWide(title);
    std::wstring wFilter = filterToWide(filter);
    std::wstring wDefExt = utf8ToWide(defaultExt);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize    = sizeof(ofn);
    ofn.hwndOwner      = hwnd;
    ofn.lpstrFilter    = wFilter.c_str();
    ofn.lpstrFile      = filePath;
    ofn.nMaxFile       = MAX_PATH;
    ofn.lpstrTitle     = wTitle.c_str();
    ofn.lpstrDefExt    = wDefExt.c_str();
    ofn.Flags          = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        return wideToUtf8(filePath);
    }
    return "";
}

} // namespace TopOpt
