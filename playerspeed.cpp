#ifndef PLAYER_SPEED_H
#define PLAYER_SPEED_H

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cmath>
#include <mutex>

// Define Vector3 for velocity
struct Vector3 {
    float x, y, z;
};

class PlayerSpeed {
private:
    HANDLE hProcess = nullptr;
    uintptr_t clientBase = 0;
    std::string lastError = "";
    std::mutex speedMutex; // Mutex for thread safety

    // Offsets
    static constexpr uintptr_t OFFSET_LOCAL_PLAYER = 0x1496FD0;
    static constexpr uintptr_t OFFSET_VELOCITY = 0x3E0;

    // Initialize process and module base
    bool initialize() {
        if (hProcess) return true; // Already initialized

        // Get CS2 process ID
        DWORD pid = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe = { sizeof(pe) };
            if (Process32First(snapshot, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, L"cs2.exe") == 0) {
                        pid = pe.th32ProcessID;
                        break;
                    }
                } while (Process32Next(snapshot, &pe));
            }
            CloseHandle(snapshot);
        }

        if (!pid) {
            lastError = "CS2 process not found";
            return false;
        }

        hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) {
            lastError = "Failed to open CS2 process";
            return false;
        }

        // Get client.dll base address
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 me = { sizeof(me) };
            if (Module32First(snapshot, &me)) {
                do {
                    if (_wcsicmp(me.szModule, L"client.dll") == 0) {
                        clientBase = (uintptr_t)me.modBaseAddr;
                        break;
                    }
                } while (Module32Next(snapshot, &me));
            }
            CloseHandle(snapshot);
        }

        if (!clientBase) {
            lastError = "Failed to find client.dll";
            CloseHandle(hProcess);
            hProcess = nullptr;
            return false;
        }

        return true;
    }

public:
    PlayerSpeed() = default;

    ~PlayerSpeed() {
        if (hProcess) {
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
    }

    // Get horizontal player speed (units per second)
    float getSpeed() {
        std::lock_guard<std::mutex> lock(speedMutex); // Correct usage

        if (!initialize()) {
            return 0.0f;
        }

        // Read local player base address
        uintptr_t localPlayerAddr = 0;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(clientBase + OFFSET_LOCAL_PLAYER), &localPlayerAddr, sizeof(localPlayerAddr), nullptr)) {
            lastError = "Failed to read local player address";
            return 0.0f;
        }

        if (!localPlayerAddr) {
            lastError = "Local player not found";
            return 0.0f;
        }

        // Read velocity
        Vector3 velocity;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(localPlayerAddr + OFFSET_VELOCITY), &velocity, sizeof(velocity), nullptr)) {
            lastError = "Failed to read velocity";
            return 0.0f;
        }

        // Calculate horizontal speed (ignore z)
        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        lastError = ""; // Clear error on success
        return speed;
    }

    // Get last error message
    std::string getLastError() const {
        // std::lock_guard<std::mutex> lock(speedMutex); // Comment out
        return lastError;
    }
};

#endif // PLAYER_SPEED_H