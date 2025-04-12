#pragma once
#include <string>

long long performance_counter_frequency();
long long performance_counter();
std::string keyToString(int vkCode);
float clampf(float val, float minVal, float maxVal);
bool isCS2WindowActive();