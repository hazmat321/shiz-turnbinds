#include "FloatingUI.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

FloatingUI::FloatingUI(SettingsManager& settings)
    : settings_(settings), hwnd_(nullptr), hFont_(nullptr),
    isDragging_(false), isScrollActive_(false), isVisible_(true) {
    RegisterWindowClass();
}

FloatingUI::~FloatingUI() {
    if (hFont_) {
        DeleteObject(hFont_);
    }
    if (hwnd_) {
        UnregisterHotKey(hwnd_, 1);
        DestroyWindow(hwnd_);
    }
}

void FloatingUI::RegisterWindowClass() {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);  // Make background transparent
    wc.lpszClassName = L"FloatingMYawUI";
    RegisterClassEx(&wc);
}

void FloatingUI::CreateFloatingWindow() {
    hwnd_ = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"FloatingMYawUI",
        L"cl_yawspeed",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, 140, 30,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );

    hFont_ = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial"
    );

    // Set layered window transparency (200 out of 255)
    SetLayeredWindowAttributes(hwnd_, 0, 200, LWA_ALPHA);

    SimulationSettings settings = settings_.get();
    int x, y;
    if (settings.windowX == -1 || settings.windowY == -1) {
        RECT rect;
        GetWindowRect(hwnd_, &rect);
        x = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;
    }
    else {
        x = settings.windowX;
        y = settings.windowY;
    }

    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    RegisterHotkey();
}

void FloatingUI::RegisterHotkey() {
    RegisterHotKey(hwnd_, 1, MOD_CONTROL | MOD_SHIFT, 'M');
}

void FloatingUI::ToggleVisibility() {
    isVisible_ = !isVisible_;
    ShowWindow(hwnd_, isVisible_ ? SW_SHOW : SW_HIDE);
}

void FloatingUI::Show() {
    CreateFloatingWindow();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void FloatingUI::UpdateMYaw(float) {
    if (isVisible_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

LRESULT CALLBACK FloatingUI::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FloatingUI* pThis = reinterpret_cast<FloatingUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<FloatingUI*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }

    if (!pThis) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Manually clear the background
        HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        SelectObject(hdc, pThis->hFont_);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));

        std::wstringstream wss;
        wss << L"cl_yawspeed: " << std::fixed << std::setprecision(1) << pThis->settings_.get().cl_yawspeed;
        DrawText(hdc, wss.str().c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        pThis->isScrollActive_ = true;
        pThis->isDragging_ = true;
        GetCursorPos(&pThis->dragStart_);
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        pThis->isScrollActive_ = false;
        pThis->isDragging_ = false;
        ReleaseCapture();

        RECT rect;
        GetWindowRect(hwnd, &rect);
        SimulationSettings settings = pThis->settings_.get();
        settings.windowX = rect.left;
        settings.windowY = rect.top;
        pThis->settings_.update([&](SimulationSettings& s) {
            s.windowX = settings.windowX;
            s.windowY = settings.windowY;
            });
        pThis->settings_.save();
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (pThis->isDragging_) {
            POINT current;
            GetCursorPos(&current);
            int dx = current.x - pThis->dragStart_.x;
            int dy = current.y - pThis->dragStart_.y;
            RECT rect;
            GetWindowRect(hwnd, &rect);
            MoveWindow(hwnd, rect.left + dx, rect.top + dy, rect.right - rect.left, rect.bottom - rect.top, TRUE);
            pThis->dragStart_ = current;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (pThis->isScrollActive_) {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float yawSpeedDelta = (delta > 0 ? 10.0f : -10.0f);
            SimulationSettings settings = pThis->settings_.get();
            float newSpeed = settings.cl_yawspeed + yawSpeedDelta;

            // Clamp between 10 and 500
            if (newSpeed < 10.0f) newSpeed = 10.0f;
            else if (newSpeed > 500.0f) newSpeed = 500.0f;

            pThis->settings_.update([&](SimulationSettings& s) {
                s.cl_yawspeed = newSpeed;
                });
            pThis->settings_.save();
            pThis->UpdateMYaw(newSpeed); // Just triggers repaint
        }
        return 0;
    }
    case WM_HOTKEY: {
        if (wParam == 1) {
            pThis->ToggleVisibility();
        }
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
