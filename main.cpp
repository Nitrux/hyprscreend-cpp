#include "display_power.hpp"
#include "lua_config.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <regex>
#include <sstream>
#include <iostream>
#include <tuple>
#include <thread>
namespace {
std::atomic_bool running{true};
void stop(int) { running = false; }

std::filesystem::path config_path() {
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".config/hyprscreend/hyprscreend.conf";
}

std::string expand_home(std::string value) {
    const char* home = std::getenv("HOME");
    const std::string marker = "$HOME";
    if (home) { const auto pos = value.find(marker); if (pos != std::string::npos) value.replace(pos, marker.size(), home); }
    return value;
}

bool external_enabled(const std::string& output, const std::string& configured) {
    if (configured.empty()) return true;
    std::istringstream names(configured); std::string name;
    while (names >> name) if (name == output) return true;
    return false;
}

bool automatic_enabled(const std::filesystem::path& path) {
    std::ifstream config(path);
    if (!config) return true;
    std::string section, line;
    const std::regex assignment(R"(^[ \t]*([A-Za-z_]+)[ \t]*=[ \t]*(.*?)[ \t]*$)");
    while (std::getline(config, line)) {
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        std::smatch match;
        if (section == "General" && std::regex_match(line, match, assignment) && match[1].str() == "Automatic") {
            auto value = match[2].str();
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value == "1" || value == "true" || value == "yes" || value == "on";
        }
    }
    return true;
}

Mode selected_mode(const Monitor& monitor, const std::string& state = {}) {
    if (monitor.virtual_display) return {1440, 900, 59.89};

    auto best = monitor.modes.front();
    for (const auto& mode : monitor.modes)
        if (mode.width > best.width || (mode.width == best.width && mode.height > best.height) ||
            (mode.width == best.width && mode.height == best.height &&
             (state.empty() ? mode.refresh > best.refresh :
              ((state == "ac") ? mode.refresh > best.refresh : mode.refresh < best.refresh))))
            best = mode;
    return best;
}

std::string mode_string(const Monitor& monitor, const Mode& mode) {
    std::ostringstream result;
    result << mode.width << "x" << mode.height << "@";
    if (monitor.virtual_display) result << std::fixed << std::setprecision(2) << mode.refresh;
    else result << static_cast<int>(mode.refresh);
    return result.str() + "Hz";
}

bool create_config(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return true;
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream out(path);
    if (!out) return false;
    out << "[General]\n"
        << "Automatic=true\n"
        << "HyprConfig=\"$HOME/.config/hypr/hyprland.lua\"\n"
        << "ScaleFactor=1.0\n\n"
        << "[Screens]\n"
        << "InternalScreen=\"eDP-1\"\n"
        << "ExternalScreen=\"\"\n";
    return static_cast<bool>(out);
}
}

int main() {
    std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
    const auto config_file = config_path();
    if (!create_config(config_file)) { std::cerr << "hyprscreend: cannot create " << config_file << '\n'; return 1; }
    std::ifstream config(config_file);
    if (!config) { std::cerr << "hyprscreend: cannot open " << config_file << '\n'; return 1; }
    std::string section; std::string hypr_config, internal = "eDP-1", external; double fallback = 1.0;
    const std::regex assignment(R"(^[ \t]*([A-Za-z_]+)[ \t]*=[ \t]*(.*?)[ \t]*$)");
    std::string line;
    while (std::getline(config, line)) {
        std::smatch match;
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        if (!std::regex_match(line, match, assignment)) continue;
        auto key = match[1].str(); auto value = match[2].str();
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
        value = expand_home(value);
        if (section == "General" && key == "HyprConfig") hypr_config = value;
        else if (section == "Screens" && key == "InternalScreen") internal = value;
        else if (section == "Screens" && key == "ExternalScreen") external = value;
        else if (section == "General" && key == "ScaleFactor") fallback = std::stod(value);
    }
    if (hypr_config.empty()) { const char* home = std::getenv("HOME"); hypr_config = std::string(home ? home : ".") + "/.config/hypr/hyprland.lua"; }
    LuaConfig lua(hypr_config);

    std::thread internal_thread([&] {
        std::string previous;
        while (running) {
            if (!automatic_enabled(config_file)) { previous.clear(); std::this_thread::sleep_for(std::chrono::seconds(1)); continue; }
            const auto state = power_state();
            if (state != "unknown" && state != previous) {
                bool ready = false;
                for (const auto& monitor : hyprland_monitors()) if (monitor.name == internal && !monitor.modes.empty()) {
                    const auto best = selected_mode(monitor, state);
                    if (lua.upsert_monitor({internal, mode_string(monitor, best), "auto", display_scale(monitor, fallback)})) hyprland_reload();
                    ready = true;
                    break;
                }
                if (ready) previous = state;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    std::thread external_thread([&] {
        while (running) {
            if (!automatic_enabled(config_file)) { std::this_thread::sleep_for(std::chrono::seconds(1)); continue; }
            for (const auto& monitor : hyprland_monitors()) if (monitor.name != internal && external_enabled(monitor.name, external) && !monitor.modes.empty()) {
                const auto best = selected_mode(monitor);
                const auto position = lua.position_for(monitor.name).value_or("auto");
                if (lua.upsert_monitor({monitor.name, mode_string(monitor, best), position, display_scale(monitor, fallback)})) hyprland_reload();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    internal_thread.join(); external_thread.join(); return 0;
}
