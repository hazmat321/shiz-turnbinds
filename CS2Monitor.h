#pragma once
#include <atomic>
#include "Settings.h"

void monitorCS2(std::atomic<bool>& running, std::atomic<bool>& shouldRun, const SettingsManager& settings);