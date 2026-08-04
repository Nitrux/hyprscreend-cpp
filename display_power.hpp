#pragma once

#include <optional>
#include <string>
#include <vector>

struct Mode { int width{}, height{}; double refresh{}; };
struct Monitor { std::string name; int width{}, height{}; bool virtual_display{}; std::vector<Mode> modes; };

std::vector<Monitor> hyprland_monitors();
std::optional<std::pair<int, int>> edid_size_mm(const std::string& output);
std::string display_scale(const Monitor& monitor, double fallback);
std::string power_state();
bool hyprland_reload();
