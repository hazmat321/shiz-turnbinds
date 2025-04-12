//#ifndef PLAYER_SPEED_H
//#define PLAYER_SPEED_H
//
//#include <windows.h>
//#include <tlhelp32.h>
//#include <string>
//#include <cmath>
//#include <mutex>
//
//struct Vector3 {
//    float x, y, z;
//};
//
//class PlayerSpeed {
//private:
//    HANDLE hProcess = nullptr;
//    uintptr_t clientBase = 0;
//    std::string lastError = "Not initialized";
//    std::mutex speedMutex;
//
//    static constexpr uintptr_t OFFSET_LOCAL_PLAYER = 0x1880000;           // Direct pawn guess
//    static constexpr uintptr_t OFFSET_LOCAL_PLAYER_CONTROLLER = 0x14A0000; // New controller guess
//    static constexpr uintptr_t OFFSET_PAWN = 0x62C;                       // m_hPawn
//    static constexpr uintptr_t OFFSET_VELOCITY = 0x400;                   // m_vecVelocity
//
//    bool initialize() {
//        if (hProcess) {
//            lastError = "Process initialized";
//            return true;
//        }
//
//        DWORD pid = 0;
//        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//        if (snapshot != INVALID_HANDLE_VALUE) {
//            PROCESSENTRY32 pe = { sizeof(pe) };
//            if (Process32First(snapshot, &pe)) {
//                do {
//                    if (_wcsicmp(pe.szExeFile, L"cs2.exe") == 0) {
//                        pid = pe.th32ProcessID;
//                        break;
//                    }
//                } while (Process32Next(snapshot, &pe));
//            }
//            CloseHandle(snapshot);
//        }
//
//        if (!pid) {
//            lastError = "CS2 process not found";
//            return false;
//        }
//
//        hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
//        if (!hProcess) {
//            lastError = "Cannot open CS2 process";
//            return false;
//        }
//
//        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
//        if (snapshot != INVALID_HANDLE_VALUE) {
//            MODULEENTRY32 me = { sizeof(me) };
//            if (Module32First(snapshot, &me)) {
//                do {
//                    if (_wcsicmp(me.szModule, L"client.dll") == 0) {
//                        clientBase = (uintptr_t)me.modBaseAddr;
//                        break;
//                    }
//                } while (Module32Next(snapshot, &me));
//            }
//            CloseHandle(snapshot);
//        }
//
//        if (!clientBase) {
//            lastError = "Cannot find client.dll";
//            CloseHandle(hProcess);
//            hProcess = nullptr;
//            return false;
//        }
//
//        lastError = "Initialized";
//        return true;
//    }
//
//public:
//    PlayerSpeed() = default;
//
//    ~PlayerSpeed() {
//        if (hProcess) {
//            CloseHandle(hProcess);
//            hProcess = nullptr;
//        }
//    }
//
//    float getSpeed() {
//        std::lock_guard<std::mutex> lock(speedMutex);
//        if (!initialize()) {
//            lastError = "Initialization failed";
//            return 0.0f;
//        }
//
//        for (int retry = 0; retry < 3; ++retry) {
//            uintptr_t pawnAddr = 0;
//            uintptr_t pawnAddrPtr = clientBase + OFFSET_LOCAL_PLAYER;
//            if (ReadProcessMemory(hProcess, (LPCVOID)pawnAddrPtr, &pawnAddr, sizeof(pawnAddr), nullptr)) {
//                if (pawnAddr) {
//                    Vector3 velocity;
//                    uintptr_t velocityAddr = pawnAddr + OFFSET_VELOCITY;
//                    if (ReadProcessMemory(hProcess, (LPCVOID)velocityAddr, &velocity, sizeof(velocity), nullptr)) {
//                        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
//                        lastError = "Speed read: " + std::to_string(speed);
//                        return speed;
//                    }
//                    lastError = "Failed to read velocity";
//                    return 0.0f;
//                }
//            }
//            lastError = retry < 2 ? "Pawn not found, retrying..." : "Pawn not found";
//            Sleep(500);
//        }
//        return 0.0f;
//    
//
//        // Fallback to controller + m_hPawn
//        uintptr_t controllerAddr = 0;
//        uintptr_t controllerAddrPtr = clientBase + OFFSET_LOCAL_PLAYER_CONTROLLER;
//        if (!ReadProcessMemory(hProcess, (LPCVOID)controllerAddrPtr, &controllerAddr, sizeof(controllerAddr), nullptr)) {
//            lastError = "Failed to read controller address";
//            return 0.0f;
//        }
//
//        if (!controllerAddr) {
//            lastError = "Controller not found";
//            return 0.0f;
//        }
//
//        uintptr_t pawnAddr = 0;
//        uintptr_t pawnAddrPtr = controllerAddr + OFFSET_PAWN;
//        if (!ReadProcessMemory(hProcess, (LPCVOID)pawnAddrPtr, &pawnAddr, sizeof(pawnAddr), nullptr)) {
//            lastError = "Failed to read pawn address";
//            return 0.0f;
//        }
//
//        if (!pawnAddr) {
//            lastError = "Pawn not found (controller)";
//            return 0.0f;
//        }
//
//        Vector3 velocity;
//        uintptr_t velocityAddr = pawnAddr + OFFSET_VELOCITY;
//        if (!ReadProcessMemory(hProcess, (LPCVOID)velocityAddr, &velocity, sizeof(velocity), nullptr)) {
//            lastError = "Failed to read velocity (controller)";
//            return 0.0f;
//        }
//
//        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
//        lastError = "Speed read (controller): " + std::to_string(speed);
//        return speed;
//    }
//
//    std::string getLastError() const {
//        //std::lock_guard<std::mutex> lock(speedMutex);
//        return lastError;
//    }
//};
//
//#endif // PLAYER_SPEED_H