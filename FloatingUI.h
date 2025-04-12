#pragma once
#include <windows.h>
#include <string>
#include "Settings.h"

// FloatingUI.h
class FloatingUI {
public:
    FloatingUI(SettingsManager& settings);
    ~FloatingUI();
    void Show(); // <- this now creates and shows the window
    void UpdateMYaw(float newYaw);
    void ToggleVisibility();
    HWND getHwnd() const { return hwnd_; } // optional
    void RegisterHotkey();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RegisterWindowClass();
    void CreateFloatingWindow(); // now called inside Show()

    HWND hwnd_;
    SettingsManager& settings_;
    HFONT hFont_;
    bool isDragging_;
    POINT dragStart_;
    bool isScrollActive_;
    bool isVisible_;
};
