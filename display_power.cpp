#include "display_power.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
std::string command(const std::string& cmd) {
    std::array<char, 4096> buffer{}; std::string out;
    FILE* pipe = ::popen(cmd.c_str(), "r"); if (!pipe) return {};
    while (std::fgets(buffer.data(), buffer.size(), pipe)) out += buffer.data();
    ::pclose(pipe); return out;
}
std::vector<std::string> json_objects(const std::string& json) {
    std::vector<std::string> out; int depth = 0; std::size_t begin = 0; bool string = false, escape = false;
    for (std::size_t i = 0; i < json.size(); ++i) {
        const char c = json[i];
        if (string) { if (escape) escape = false; else if (c == 92) escape = true; else if (c == '"') string = false; continue; }
        if (c == '"') string = true;
        else if (c == '{') { if (depth == 0) begin = i; ++depth; }
        else if (c == '}' && depth > 0) { --depth; if (depth == 0) out.push_back(json.substr(begin, i - begin + 1)); }
    }
    return out;
}
std::vector<Mode> modes_in(const std::string& object) {
    std::vector<Mode> modes;
    const std::regex quoted(R"("(\d+)x(\d+)@([0-9.]+)Hz")");
    for (auto i = std::sregex_iterator(object.begin(), object.end(), quoted); i != std::sregex_iterator{}; ++i)
        modes.push_back({std::stoi((*i)[1]), std::stoi((*i)[2]), std::stod((*i)[3])});
    if (!modes.empty()) return modes;
    const std::regex structured(R"("width"\s*:\s*(\d+).*?"height"\s*:\s*(\d+).*?(?:"refreshRate"|"refresh")\s*:\s*([0-9.]+))");
    for (auto i = std::sregex_iterator(object.begin(), object.end(), structured); i != std::sregex_iterator{}; ++i)
        modes.push_back({std::stoi((*i)[1]), std::stoi((*i)[2]), std::stod((*i)[3]) > 1000 ? std::stod((*i)[3]) / 1000 : std::stod((*i)[3])});
    return modes;
}
}

std::vector<Monitor> hyprland_monitors() {
    std::vector<Monitor> result;
    const std::regex name_re("\"name\"[ \\t]*:[ \\t]*\"([^\"]+)\"");
    for (const auto& object : json_objects(command("hyprctl monitors -j 2>/dev/null"))) {
        std::smatch match; if (!std::regex_search(object, match, name_re)) continue;
        Monitor monitor{}; monitor.name = match[1].str(); monitor.modes = modes_in(object);
        const std::regex size_re(R"("width"\s*:\s*(\d+).*?"height"\s*:\s*(\d+))");
        if (std::regex_search(object, match, size_re)) { monitor.width = std::stoi(match[1]); monitor.height = std::stoi(match[2]); }
        result.push_back(std::move(monitor));
    } return result;
}

std::optional<std::pair<int, int>> edid_size_mm(const std::string& output) {
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm", std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.path().filename().string().find('-' + output) == std::string::npos) continue;
        std::ifstream edid(entry.path() / "edid", std::ios::binary); std::string data((std::istreambuf_iterator<char>(edid)), {});
        if (data.size() >= 23 && static_cast<unsigned char>(data[21]) && static_cast<unsigned char>(data[22]))
            return std::pair{static_cast<int>(static_cast<unsigned char>(data[21])) * 10, static_cast<int>(static_cast<unsigned char>(data[22])) * 10};
    } return std::nullopt;
}

std::string display_scale(const Monitor& monitor, double fallback) {
    const auto physical = edid_size_mm(monitor.name); if (!physical) return std::to_string(fallback);
    int w = monitor.width, h = monitor.height;
    for (const auto& mode : monitor.modes) if (mode.width > w || (mode.width == w && mode.height > h)) { w = mode.width; h = mode.height; }
    if (w <= 0 || h <= 0) return std::to_string(fallback);
    const double ppi = std::hypot(w, h) / (std::hypot(physical->first, physical->second) / 25.4);
    return ppi >= 200 ? "2" : ppi >= 160 ? "1.5" : "1";
}

std::string power_state() {
    bool found = false;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/power_supply", std::filesystem::directory_options::skip_permission_denied)) {
        std::ifstream type_file(entry.path() / "type"); std::string type; std::getline(type_file, type);
        if (type != "Mains" && type != "AC" && type != "USB" && type != "USB_C" && type != "USB_PD") continue;
        found = true; std::ifstream online(entry.path() / "online"); std::string value; std::getline(online, value); if (value == "1") return "ac";
    }
    if (found) return "battery";
    const auto upower = command("upower -i $(upower -e 2>/dev/null | grep DisplayDevice | head -n 1) 2>/dev/null");
    if (upower.find("on-battery: yes") != std::string::npos) return "battery";
    if (upower.find("on-battery: no") != std::string::npos) return "ac";
    return "unknown";
}

bool hyprland_reload() { return std::system("hyprctl reload >/dev/null 2>&1") == 0; }
