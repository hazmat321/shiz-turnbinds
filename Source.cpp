#define _WIN32_WINNT 0x0600 // Windows Vista+ for MIB_TCPTABLE2
#define NOMINMAX

#include <winsock2.h>       // Winsock 2, before windows.h
#include <ws2tcpip.h>       // For inet_ntop, INET_ADDRSTRLEN
#include <iphlpapi.h>       // For GetTcpTable2, MIB_TCPTABLE2
#include <windows.h>        // Windows API
#include <iostream>
#include <conio.h>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <cmath>
#include <mutex>
#include <iomanip>
#include <algorithm>
#include <mmsystem.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <memory> /

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

std::string debugMsg = "";
std::mutex debugMutex;

// Define color constants at the top of your file with the other globals
const WORD COLOR_DEFAULT = 7;  // Default console color (typically white on black)
const WORD COLOR_BLUE = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD COLOR_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD COLOR_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD COLOR_GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD COLOR_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
const WORD COLOR_PURPLE = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD COLOR_CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

// Helper function to set text color
void setTextColor(WORD color) {
    SetConsoleTextAttribute(hConsole, color);
}

void printColored(const std::string& text, WORD color) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalAttributes = csbi.wAttributes;

    setTextColor(color);
    std::cout << text;
    setTextColor(originalAttributes);
}

// In the global declarations section, modify SimulationSettings struct
struct SimulationSettings {
    float m_yaw;
    float cl_yawspeed;
    int leftKey;
    int rightKey;
    float updateRate;
    bool autoActivate;
    float modifier;    // New: Modifier value
    int modifierKey;   // New: Modifier key
};

SimulationSettings settings = { 0.022f, 210.0f, 0x06, 0x05, 1000.0f, true, 0.5f, 0x12 }; // Modifier 0.5, modifierKey Alt (VK_MENU)
std::mutex settingsMutex;

const std::string CONFIG_FILE = "settings.json";

// Console handling
COORD settingsStartPos = { static_cast<SHORT>(0), static_cast<SHORT>(0) };
COORD statusPos = { static_cast<SHORT>(0), static_cast<SHORT>(0) };
COORD debugPos = { static_cast<SHORT>(0), static_cast<SHORT>(0) };
COORD autoActivatePos = { static_cast<SHORT>(0), static_cast<SHORT>(0) }; // Position for auto-activate status

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

const long long PERF_FREQ = performance_counter_frequency();

void updateDebugMsg(const std::string& msg) {
    std::lock_guard<std::mutex> lock(debugMutex);
    debugMsg = msg;
}

std::string getDebugMsg() {
    std::lock_guard<std::mutex> lock(debugMutex);
    return debugMsg;
}

// In saveSettings function
void saveSettings() {
    std::lock_guard<std::mutex> lock(settingsMutex);
    json j;
    j["m_yaw"] = settings.m_yaw;
    j["cl_yawspeed"] = settings.cl_yawspeed;
    j["leftKey"] = settings.leftKey;
    j["rightKey"] = settings.rightKey;
    j["updateRate"] = settings.updateRate;
    j["autoActivate"] = settings.autoActivate;
    j["modifier"] = settings.modifier;        // New
    j["modifierKey"] = settings.modifierKey;  // New

    std::ofstream file(CONFIG_FILE);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
    else {
        std::cerr << "Failed to save settings to " << CONFIG_FILE << "\n";
    }
}

const std::string WHITELIST_FILE = "whitelist.json";



// In loadSettings function
void loadSettings() {
    std::ifstream file(CONFIG_FILE);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            std::lock_guard<std::mutex> lock(settingsMutex);
            settings.m_yaw = j.value("m_yaw", settings.m_yaw);
            settings.cl_yawspeed = j.value("cl_yawspeed", settings.cl_yawspeed);
            settings.leftKey = j.value("leftKey", settings.leftKey);
            settings.rightKey = j.value("rightKey", settings.rightKey);
            settings.updateRate = j.value("updateRate", settings.updateRate);
            settings.autoActivate = j.value("autoActivate", settings.autoActivate);
            settings.modifier = j.value("modifier", settings.modifier);          // New
            settings.modifierKey = j.value("modifierKey", settings.modifierKey); // New
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to parse " << CONFIG_FILE << ": " << e.what() << "\n";
        }
        file.close();
    }
}

void moveMouse(int deltaX) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = deltaX;
    SendInput(1, &input, sizeof(INPUT));
}

std::string keyToString(int vkCode) {
    if (vkCode == 0x05) return "Mouse5";
    if (vkCode == 0x06) return "Mouse4";
    if (vkCode >= 'A' && vkCode <= 'Z') return std::string(1, static_cast<char>(vkCode));
    return "VK_0x" + std::to_string(vkCode);
}

float clampf(float val, float minVal, float maxVal) {
    return std::max(minVal, std::min(maxVal, val));
}

bool isCS2WindowActive() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    char windowTitle[256];
    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
    std::string title(windowTitle);

    return title.find("Counter-Strike") != std::string::npos;
}

std::vector<std::string> loadWhitelist() {
    std::vector<std::string> servers;
    std::ifstream file(WHITELIST_FILE);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            if (j.contains("servers") && j["servers"].is_array()) {
                servers = j["servers"].get<std::vector<std::string>>();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to parse " << WHITELIST_FILE << ": " << e.what() << "\n";
        }
        file.close();
    }
    else {
        std::cerr << "Could not open " << WHITELIST_FILE << "\n";
    }
    return servers;
}

bool isConnectedToServer(const std::vector<std::string>& targetIPs) {
    if (targetIPs.empty()) return true; // Allow activation if whitelist is empty
    PMIB_TCPTABLE2 tcpTable = nullptr;
    ULONG size = 0;
    DWORD result = GetTcpTable2(tcpTable, &size, FALSE);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
        tcpTable = (PMIB_TCPTABLE2)malloc(size);
        if (!tcpTable) return false;
        result = GetTcpTable2(tcpTable, &size, FALSE);
    }
    if (result != NO_ERROR) {
        free(tcpTable);
        return false;
    }
    bool found = false;
    for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
        MIB_TCPROW2 row = tcpTable->table[i]; // Changed 'Table' to 'table'
        if (row.dwState == MIB_TCP_STATE_ESTAB) { // Use TcpStateEstablished (value 5) instead
            char remoteAddr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &row.dwRemoteAddr, remoteAddr, sizeof(remoteAddr))) {
                if (std::find(targetIPs.begin(), targetIPs.end(), remoteAddr) != targetIPs.end()) {
                    found = true;
                    break;
                }
            }
        }
    }
    free(tcpTable);
    return found;
}

void cs2Monitor(std::atomic<bool>& running, std::atomic<bool>& shouldRunSim) {
    bool lastActive = false;
    std::vector<std::string> whitelist = loadWhitelist();

    while (shouldRunSim) {
        bool cs2Active = isCS2WindowActive();
        bool serverConnected = isConnectedToServer(whitelist);

        if (cs2Active && serverConnected && !lastActive) {
            running = true;
        }
        else if (!cs2Active || !serverConnected) {
            running = false;
        }

        lastActive = cs2Active && serverConnected;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// In mouseSimulation function, update variables and logic
void mouseSimulation(std::atomic<bool>& running) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    float m_yaw = 0.022f, cl_yawspeed = 210.0f, updateRate = 1000.0f, modifier = 0.5f;
    int leftKey = 0x06, rightKey = 0x05, modifierKey = 0x12;

    double remaining = 0.0;
    double currentSpeed = 0.0;
    long long lastTime = performance_counter();
    bool lastLeft = false, lastRight = false;

    std::vector<std::string> whitelist = loadWhitelist(); // Load whitelist at start

    while (true) {
        if (!running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            currentSpeed = 0.0;
            remaining = 0.0;
            updateDebugMsg("");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(settingsMutex);
            m_yaw = settings.m_yaw;
            cl_yawspeed = settings.cl_yawspeed;
            leftKey = settings.leftKey;
            rightKey = settings.rightKey;
            updateRate = settings.updateRate;
            modifier = settings.modifier;
            modifierKey = settings.modifierKey;
        }

        if (!isCS2WindowActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        bool leftDown = (GetAsyncKeyState(leftKey) & 0x8000) != 0;
        bool rightDown = (GetAsyncKeyState(rightKey) & 0x8000) != 0;
        bool modifierDown = (GetAsyncKeyState(modifierKey) & 0x8000) != 0; // New
        int direction = 0;
        std::string keyStatus;

        if (leftDown && !rightDown) direction = -1, keyStatus = keyToString(leftKey);
        else if (rightDown && !leftDown) direction = 1, keyStatus = keyToString(rightKey);
        else if (leftDown && rightDown) direction = 0, keyStatus = "Both keys pressed";

        long long currentTime = performance_counter();
        double deltaTime = static_cast<double>(currentTime - lastTime) / PERF_FREQ;
        lastTime = currentTime;

        if (leftDown != lastLeft || rightDown != lastRight || deltaTime >= (1.0 / updateRate)) {
            if (direction != 0) {
                double effectiveYawSpeed = cl_yawspeed * (modifierDown ? modifier : 1.0);
                double targetSpeed = direction * effectiveYawSpeed * (1.0 / m_yaw);
                currentSpeed += (targetSpeed - currentSpeed) * 0.15;

                double moveAmount = currentSpeed * deltaTime;
                if (std::abs(moveAmount) > 100.0)
                    moveAmount = std::copysign(100.0, moveAmount);

                remaining += moveAmount;

                int intMove = static_cast<int>(std::round(remaining));
                if (std::abs(intMove) >= 1) {
                    moveMouse(intMove);
                    remaining -= intMove;
                }

                updateDebugMsg(keyStatus + " | m_yaw: " + std::to_string(m_yaw) +
                    ", cl_yawspeed: " + std::to_string(effectiveYawSpeed) +
                    ", move: " + std::to_string(moveAmount) +
                    ", remaining: " + std::to_string(remaining) +
                    ", modifier: " + (modifierDown ? "ON" : "OFF") +
                    ", server: " + (isConnectedToServer(whitelist) ? "Whitelisted" : "Not Whitelisted"));
            }
            else {
                currentSpeed = 0.0;
                remaining = 0.0;
                updateDebugMsg("");
            }
        }

        lastLeft = leftDown;
        lastRight = rightDown;

        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(1e6 / updateRate)));
    }
}


// Thread for monitoring CS2 and auto-activating the simulation
void autoActivationMonitor(std::atomic<bool>& running, std::atomic<bool>& simRunning) {
    bool lastCS2State = false;
    bool autoActivate = true;

    while (running) {
        {
            std::lock_guard<std::mutex> lock(settingsMutex);
            autoActivate = settings.autoActivate;
        }

        bool cs2Active = isCS2WindowActive();

        // Only auto-activate/deactivate if the setting is enabled
        if (autoActivate) {
            if (cs2Active && !lastCS2State) {
                // CS2 just became active
                simRunning = true;
            }
            else if (!cs2Active && lastCS2State) {
                // CS2 just became inactive
                simRunning = false;
            }
        }

        lastCS2State = cs2Active;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void writeAtPosition(const COORD& pos, const std::string& text, bool clearLine = true) {
    SetConsoleCursorPosition(hConsole, pos);
    if (clearLine) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        DWORD written;
        FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X - pos.X, pos, &written);
        SetConsoleCursorPosition(hConsole, pos);
    }
    std::cout << text << std::flush;
}

int detectKeyPress() {
    COORD tempPos = { 0, debugPos.Y + 2 };
    writeAtPosition(tempPos, "Press a key or mouse button to assign...");
    Sleep(250);
    while (_kbhit()) _getch();
    while (true) {
        for (int vk = 0x01; vk <= 0xFE; ++vk) {
            if (vk == VK_RETURN) continue;
            if (GetAsyncKeyState(vk) & 0x8000) {
                while (GetAsyncKeyState(vk) & 0x8000) Sleep(10);
                writeAtPosition(tempPos, "Detected: " + keyToString(vk));
                Sleep(1000);
                writeAtPosition(tempPos, "");
                return vk;
            }
        }
        Sleep(10);
    }
}

// In updateSettingsDisplay function
void updateSettingsDisplay(int selectedOption, const SimulationSettings& localSettings) {
    const char* options[] = { "m_yaw", "cl_yawspeed", "Left Key", "Right Key", "Update Rate (Hz)", "Auto-Activate", "Modifier", "Modifier Key" }; // Added new options

    for (int i = 0; i < 8; ++i) { // Updated to 8 options
        COORD pos = { 0, static_cast<SHORT>(settingsStartPos.Y + i) };
        std::string prefix = (i == selectedOption) ? "> " : "  ";
        std::string value;

        if (i == 0) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << localSettings.m_yaw;
            value = oss.str();
        }
        else if (i == 1) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << localSettings.cl_yawspeed;
            value = oss.str();
        }
        else if (i == 2) value = keyToString(localSettings.leftKey);
        else if (i == 3) value = keyToString(localSettings.rightKey);
        else if (i == 4) {
            std::ostringstream oss;
            oss << static_cast<int>(localSettings.updateRate) << " Hz";
            value = oss.str();
        }
        else if (i == 5) {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            WORD originalAttributes = csbi.wAttributes;

            writeAtPosition(pos, prefix + options[i] + ": ", false);

            if (localSettings.autoActivate) {
                SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                std::cout << "On";
            }
            else {
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::cout << "Off";
            }

            SetConsoleTextAttribute(hConsole, originalAttributes);
            continue;
        }
        else if (i == 6) { // New: Modifier value
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << localSettings.modifier;
            value = oss.str();
        }
        else if (i == 7) { // New: Modifier key
            value = keyToString(localSettings.modifierKey);
        }

        writeAtPosition(pos, prefix + options[i] + ": " + value);
    }
}

void updateStatusDisplay(bool running) {
    // Save current text attributes
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalAttributes = csbi.wAttributes;

    // Position cursor at the status position
    SetConsoleCursorPosition(hConsole, statusPos);

    // Clear the line
    DWORD written;
    FillConsoleOutputCharacter(hConsole, ' ', 50, statusPos, &written);
    SetConsoleCursorPosition(hConsole, statusPos);

    // Write "Status: " with original color
    std::cout << "Status: ";

    // Set color based on running state (green for running, red for stopped)
    if (running) {
        // Green text (FOREGROUND_GREEN | FOREGROUND_INTENSITY for bright green)
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "Running";
    }
    else {
        // Red text (FOREGROUND_RED | FOREGROUND_INTENSITY for bright red)
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "Stopped";
    }

    // Restore original text attributes
    SetConsoleTextAttribute(hConsole, originalAttributes);

    std::cout << std::flush;
}

void updateDebugDisplay(const std::string& debugMsg) {
    writeAtPosition(debugPos, "Debug: " + debugMsg);
}

// Function to update the CS2 detection status display
void updateCS2StatusDisplay(bool detected) {
    COORD cs2StatusPos = { 0, static_cast<SHORT>(debugPos.Y + 1) };

    // Save current text attributes
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalAttributes = csbi.wAttributes;

    // Position cursor and clear the line
    SetConsoleCursorPosition(hConsole, cs2StatusPos);
    DWORD written;
    FillConsoleOutputCharacter(hConsole, ' ', 50, cs2StatusPos, &written);
    SetConsoleCursorPosition(hConsole, cs2StatusPos);

    // Write "CS2: " with original color
    std::cout << "CS2: ";

    // Set color based on detection state
    if (detected) {
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "Detected";
    }
    else {
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "Not Detected";
    }

    // Restore original text attributes
    SetConsoleTextAttribute(hConsole, originalAttributes);
    std::cout << std::flush;
}

// Function to draw a horizontal separator line
void drawSeparatorLine(const COORD& pos) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    // Calculate console width
    int consoleWidth = csbi.dwSize.X;

    // Create a string of the appropriate length filled with separator characters
    std::string separator(consoleWidth - 1, '-');

    // Position cursor and output the separator
    SetConsoleCursorPosition(hConsole, pos);
    std::cout << separator << std::flush;
}

// Usage example:
void displayInstructions() {
    printColored("ARROW KEYS: ", COLOR_WHITE);
    std::cout << "Move to property\n";

    printColored("ENTER KEY: ", COLOR_WHITE);
    std::cout << "Select Left/Right key hit to switch bind\n";

    printColored("QUIT: ", COLOR_WHITE);
    std::cout << "Q\n";

    std::cout << "\n";
    printColored("CONFIG:\n", COLOR_BLUE);
}

int main() {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    SetConsoleMode(hConsole, consoleMode & ~ENABLE_QUICK_EDIT_MODE);

    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    timeBeginPeriod(1);

    loadSettings();

    std::atomic<bool> running(false);
    std::atomic<bool> shouldRunSim(true);
    std::thread simThread(mouseSimulation, std::ref(running));
    simThread.detach();

    std::thread monitorThread(cs2Monitor, std::ref(running), std::ref(shouldRunSim));
    monitorThread.detach();

    int selectedOption = 0;
    std::string lastDebugMsg;

    // Save current text attributes
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalAttributes = csbi.wAttributes;

    // Set light purple color
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    printColored(R"(
 _   _    _    _   _  ____ _  ___    ____  _   _ ____  _____
| | | |  / \  | \ | |/ ___/ |/ _ \  / ___|| | | |  _ \|  ___|
| |_| | / _ \ |  \| | |  _| | | | | \___ \| | | | |_) | |_   
|  _  |/ ___ \| |\  | |_| | | |_| |  ___) | |_| |  _ <|  _|  
|_| |_/_/   \_\_| \_|\____|_|\___/  |____/ \___/|_| \_\_|    
             turnbinds by Shizangle @ hang10.surf/discord
)", COLOR_CYAN);
    SetConsoleTextAttribute(hConsole, originalAttributes);

    displayInstructions();

    // Set console positions with adjusted spacing
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    settingsStartPos = { 0, csbi.dwCursorPosition.Y };
    statusPos = { 0, static_cast<SHORT>(settingsStartPos.Y + 10) }; // Moved down to accommodate 8 settings + spacing
    debugPos = { 0, static_cast<SHORT>(statusPos.Y + 2) };          // Below status with extra space
    COORD separatorPos = { 0, static_cast<SHORT>(settingsStartPos.Y + 9) }; // Separator after settings

    SimulationSettings localSettings;
    {
        std::lock_guard<std::mutex> lock(settingsMutex);
        localSettings = settings;
    }

    // Draw separator line after settings
    drawSeparatorLine(separatorPos);

    updateSettingsDisplay(selectedOption, localSettings);
    updateStatusDisplay(running);
    updateDebugDisplay(getDebugMsg());
    updateCS2StatusDisplay(isCS2WindowActive());

    while (true) {
        {
            std::lock_guard<std::mutex> lock(settingsMutex);
            localSettings = settings;
        }

        if (_kbhit()) {
            int ch = _getch();
            bool settingsChanged = false;

            if (ch == 224) {
                ch = _getch();
                if (ch == 72) selectedOption = (selectedOption - 1 + 8) % 8; // Up
                else if (ch == 80) selectedOption = (selectedOption + 1) % 8; // Down
                else if (ch == 75 || ch == 77) { // Left/Right
                    std::lock_guard<std::mutex> lock(settingsMutex);
                    float delta = (ch == 75 ? -1 : 1);
                    if (selectedOption == 0) settings.m_yaw = clampf(settings.m_yaw + delta * 0.001f, 0.001f, 0.1f);
                    else if (selectedOption == 1) settings.cl_yawspeed = clampf(settings.cl_yawspeed + delta * 10.0f, 10.0f, 500.0f);
                    else if (selectedOption == 4) settings.updateRate = clampf(settings.updateRate + delta * 100.0f, 100.0f, 2000.0f);
                    else if (selectedOption == 6) settings.modifier = clampf(settings.modifier + delta * 0.1f, 0.1f, 2.0f);
                    settingsChanged = true;
                    localSettings = settings;
                }
            }
            else if (ch == 13) { // Enter key
                if (selectedOption == 2 || selectedOption == 3 || selectedOption == 7) {
                    int newKey = detectKeyPress();
                    std::lock_guard<std::mutex> lock(settingsMutex);
                    if (selectedOption == 2) settings.leftKey = newKey;
                    else if (selectedOption == 3) settings.rightKey = newKey;
                    else if (selectedOption == 7) settings.modifierKey = newKey;
                    settingsChanged = true;
                    localSettings = settings;
                }
                else if (selectedOption == 5) { // Toggle auto-activate
                    std::lock_guard<std::mutex> lock(settingsMutex);
                    settings.autoActivate = !settings.autoActivate;
                    settingsChanged = true;
                    localSettings = settings;
                }
            }
            else if (ch == 'q' || ch == 'Q') {
                shouldRunSim = false;
                running = false;
                break;
            }

            if (settingsChanged) {
                saveSettings();
            }

            updateSettingsDisplay(selectedOption, localSettings);
        }

        // Only refresh status if it changed
        static bool lastRunning = !running;
        if (lastRunning != running) {
            updateStatusDisplay(running);
            lastRunning = running;
        }

        std::string currentDebug = getDebugMsg();
        if (currentDebug != lastDebugMsg) {
            updateDebugDisplay(currentDebug);
            lastDebugMsg = currentDebug;
        }

        // Update CS2 status periodically
        static bool lastCS2Status = false;
        bool currentCS2Status = isCS2WindowActive();
        if (currentCS2Status != lastCS2Status) {
            updateCS2StatusDisplay(currentCS2Status);
            lastCS2Status = currentCS2Status;
        }

        Sleep(10);
    }

    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
    SetConsoleMode(hConsole, consoleMode);
    timeEndPeriod(1);

    return 0;
}