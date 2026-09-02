// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

struct MonitorUpdate {
    std::string output;
    std::string mode;
    std::string position;
    std::string scale;
};

class LuaConfig {
public:
    explicit LuaConfig(std::filesystem::path path);
    std::optional<std::string> position_for(const std::string& output) const;
    bool upsert_monitor(const MonitorUpdate& update);

private:
    std::filesystem::path path_;
    mutable std::mutex mutex_;
};
