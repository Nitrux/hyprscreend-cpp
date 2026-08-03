#include "display_power.hpp"
#include "lua_config.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <tuple>
#include <thread>

namespace { std::atomic_bool running{true}; void stop(int) { running = false; } }

int main() {
    std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
    const char* config_path = std::getenv("HYPRSCREEND_CONFIG");
    const std::string config_file = config_path ? config_path : "/etc/hyprscreend.conf";
    std::ifstream config(config_file); if (!config) { std::cerr << "hyprscreend: cannot open " << config_file << '\n'; return 1; }
    std::string hypr_config, internal = "eDP-1"; double fallback = 1.0; std::string key, value;
    while (config >> key >> value) {
        if (!value.empty() && value.front() == '"') value = value.substr(1, value.size() - (value.back() == '"' ? 2 : 1));
        if (key == "HYPR_CONFIG") hypr_config = value;
        else if (key == "INTERNAL_SCREEN") internal = value;
        else if (key == "SCALE_FACTOR") fallback = std::stod(value);
    }
    if (hypr_config.empty()) { const char* home = std::getenv("HOME"); hypr_config = std::string(home ? home : "") + "/.config/hypr/hyprland.lua"; }
    LuaConfig lua(hypr_config);

    std::thread internal_thread([&] {
        std::string previous;
        while (running) {
            const auto state = power_state();
            if (state != "unknown" && state != previous) {
                for (const auto& monitor : hyprland_monitors()) if (monitor.name == internal && !monitor.modes.empty()) {
                    auto best = monitor.modes.front();
                    for (const auto& mode : monitor.modes) if (mode.width > best.width || (mode.width == best.width && mode.height > best.height) || (mode.width == best.width && mode.height == best.height && ((state == "ac") ? mode.refresh > best.refresh : mode.refresh < best.refresh))) best = mode;
                    if (lua.upsert_monitor({internal, std::to_string(best.width) + "x" + std::to_string(best.height) + "@" + std::to_string(static_cast<int>(best.refresh)) + "Hz", "auto", display_scale(monitor, fallback)})) hyprland_reload();
                    break;
                } previous = state;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    std::thread external_thread([&] {
        while (running) {
            for (const auto& monitor : hyprland_monitors()) if (monitor.name != internal && !monitor.modes.empty()) {
                auto best = *std::max_element(monitor.modes.begin(), monitor.modes.end(), [](const Mode& a, const Mode& b) { return std::tie(a.width, a.height, a.refresh) < std::tie(b.width, b.height, b.refresh); });
                const auto position = lua.position_for(monitor.name).value_or("auto");
                if (lua.upsert_monitor({monitor.name, std::to_string(best.width) + "x" + std::to_string(best.height) + "@" + std::to_string(static_cast<int>(best.refresh)) + "Hz", position, display_scale(monitor, fallback)})) hyprland_reload();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    internal_thread.join(); external_thread.join(); return 0;
}
