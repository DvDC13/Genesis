#pragma once

#include <format>
#include <string_view>

namespace Genesis {

class Logger {
public:
    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        log("INFO", Color::Green, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        log("WARN", Color::Yellow, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        log("ERROR", Color::Red, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    enum class Color { Green, Yellow, Red };
    static void log(std::string_view level, Color color, const std::string& message);
};

} // namespace Genesis
