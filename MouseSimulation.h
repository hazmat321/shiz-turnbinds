#pragma once
#include "Settings.h"
#include <atomic>

class MouseSimulator {
public:
    void run(std::atomic<bool>& running, const SettingsManager& settings, std::function<void(const std::string&)> debugCallback);

private:
    void moveMouse(int deltaX);
};