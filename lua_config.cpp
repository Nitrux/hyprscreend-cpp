// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "lua_config.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
struct Block { std::size_t begin{}, end{}; std::string output; std::string position; };

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read Hyprland configuration: " + path.string());
    std::ostringstream data;
    data << in.rdbuf();
    return data.str();
}

std::string value(const std::string& block, const char* key) {
    const std::regex re(std::string("(^|\\n)[ \\t]*") + key + "[ \\t]*=[ \\t]*\"([^\"]*)\"");
    std::smatch match;
    return std::regex_search(block, match, re) ? match[2].str() : std::string{};
}

std::vector<Block> blocks(const std::string& text) {
    std::vector<Block> result;
    const std::regex start(R"(hl\.monitor[ \t]*\([ \t]*\{[ \t]*(\r?\n|$))");
    auto begin = std::sregex_iterator(text.begin(), text.end(), start);
    for (auto it = begin; it != std::sregex_iterator{}; ++it) {
        const auto line_start = text.rfind('\n', it->position() == 0 ? 0 : static_cast<std::size_t>(it->position()) - 1);
        const std::size_t pos = text.find_first_not_of(" \t", line_start == std::string::npos ? 0 : line_start + 1);
        const std::size_t next = static_cast<std::size_t>(it->position() + it->length());
        const std::size_t close = text.find("})", next);
        if (close == std::string::npos) continue;
        const std::size_t end = close + 2;
        const std::string body = text.substr(pos, end - pos);
        result.push_back({pos, end, value(body, "output"), value(body, "position")});
    }
    return result;
}

std::string replacement(const MonitorUpdate& update, std::string indent) {
    std::ostringstream out;
    out << indent << "hl.monitor({\n"
        << indent << "    output = \"" << update.output << "\",\n"
        << indent << "    mode = \"" << update.mode << "\",\n"
        << indent << "    position = \"" << update.position << "\",\n"
        << indent << "    scale = " << update.scale << ",\n"
        << indent << "})";
    return out.str();
}

void atomic_write(const std::filesystem::path& path, const std::string& data) {
    const auto dir = path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    std::string pattern = (dir / (path.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    const int fd = ::mkstemp(name.data());
    if (fd < 0) throw std::runtime_error("cannot create temporary Hyprland configuration");
    try {
        struct stat original{};
        if (::stat(path.c_str(), &original) == 0) ::fchmod(fd, original.st_mode & 07777);
        std::size_t written = 0;
        while (written < data.size()) {
            const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
            if (n <= 0) throw std::runtime_error("cannot write temporary Hyprland configuration");
            written += static_cast<std::size_t>(n);
        }
        if (::fsync(fd) != 0 || ::close(fd) != 0) throw std::runtime_error("cannot sync temporary configuration");
        std::filesystem::rename(name.data(), path);
        const int dirfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd >= 0) { ::fsync(dirfd); ::close(dirfd); }
    } catch (...) {
        ::close(fd);
        std::error_code ignored;
        std::filesystem::remove(name.data(), ignored);
        throw;
    }
}
}

LuaConfig::LuaConfig(std::filesystem::path path) : path_(std::move(path)) {}

std::optional<std::string> LuaConfig::position_for(const std::string& output) const {
    std::lock_guard lock(mutex_);
    const auto text = read_file(path_);
    for (const auto& block : blocks(text))
        if (block.output == output && !block.position.empty()) return block.position;
    return std::nullopt;
}

bool LuaConfig::upsert_monitor(const MonitorUpdate& update) {
    std::lock_guard lock(mutex_);
    const std::string text = read_file(path_);
    const auto found = blocks(text);
    if (found.empty()) {
        const std::string inserted = replacement(update, "");
        atomic_write(path_, inserted + (text.empty() || text.back() == '\n' ? "" : "\n") + text);
        return true;
    }

    std::optional<Block> selected;
    for (const auto& block : found) if (block.output == update.output) { selected = block; break; }
    if (!selected) for (const auto& block : found) if (block.output.empty()) { selected = block; break; }
    if (!selected) {
        const auto& first = found.front();
        const auto indent = std::string(text.substr(first.begin, text.find('h', first.begin) - first.begin));
        atomic_write(path_, text.substr(0, first.begin) + replacement(update, indent) + "\n" + text.substr(first.begin));
        return true;
    }
    const std::string original = text.substr(selected->begin, selected->end - selected->begin);
    const auto line_start = text.rfind('\n', selected->begin == 0 ? 0 : selected->begin - 1);
    const auto first_nonspace = text.find_first_not_of(" \t", line_start == std::string::npos ? 0 : line_start + 1);
    const std::string indent = text.substr(line_start == std::string::npos ? 0 : line_start + 1,
                                           first_nonspace - (line_start == std::string::npos ? 0 : line_start + 1));
    atomic_write(path_, text.substr(0, selected->begin) + replacement(update, indent) + text.substr(selected->end));
    return original != replacement(update, indent);
}
