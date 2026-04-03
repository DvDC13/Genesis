#include "core/Logger.h"

#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace Genesis {

void Logger::log(std::string_view level, Color color, const std::string& message) {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD colorAttr;
    switch (color) {
        case Color::Green:  colorAttr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case Color::Yellow: colorAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case Color::Red:    colorAttr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
    }
    SetConsoleTextAttribute(console, colorAttr);
    std::cout << "[" << level << "] ";
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    std::cout << message << "\n";
#else
    const char* colorCode;
    switch (color) {
        case Color::Green:  colorCode = "\033[32m"; break;
        case Color::Yellow: colorCode = "\033[33m"; break;
        case Color::Red:    colorCode = "\033[31m"; break;
    }
    std::cout << colorCode << "[" << level << "] \033[0m" << message << "\n";
#endif
}

} // namespace Genesis
