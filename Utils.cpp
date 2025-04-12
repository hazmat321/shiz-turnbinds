#include "Utils.h"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <string>

long long performance_counter_frequency() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
}

long long performance_counter() {
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

std::string keyToString(int vkCode) {
    if (vkCode == 0x05) return "Mouse5";
    if (vkCode == 0x06) return "Mouse4";
    if (vkCode >= 'A' && vkCode <= 'Z') return std::string(1, static_cast<char>(vkCode));
    return "VK_0x" + std::to_string(vkCode);
}

float clampf(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

bool isCS2WindowActive() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return false;
    }
    char windowTitle[256];
    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
    std::string title(windowTitle);
    return title.find("Counter-Strike") != std::string::npos;
}