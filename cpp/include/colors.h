// \033 = ESC (ASCII 27). Terminal interprets \033[<code>m as a color instruction. (INFORMATION)
// \033[0m resets back to normal text. Works on Linux/macOS terminals and WSL.     (INFORMATION)

#pragma once
#include <string>

namespace Color {
    // Standard text colors
    inline std::string green(const std::string& s)   { return "\033[32m" + s + "\033[0m"; }
    inline std::string red(const std::string& s)     { return "\033[31m" + s + "\033[0m"; }
    inline std::string yellow(const std::string& s)  { return "\033[33m" + s + "\033[0m"; }
    inline std::string cyan(const std::string& s)    { return "\033[36m" + s + "\033[0m"; }
    inline std::string blue(const std::string& s)    { return "\033[34m" + s + "\033[0m"; }
    inline std::string magenta(const std::string& s) { return "\033[35m" + s + "\033[0m"; }
    inline std::string white(const std::string& s)   { return "\033[37m" + s + "\033[0m"; }

    // Text styles
    inline std::string bold(const std::string& s)      { return "\033[1m"  + s + "\033[0m"; }
    inline std::string dim(const std::string& s)       { return "\033[2m"  + s + "\033[0m"; }
    inline std::string underline(const std::string& s) { return "\033[4m"  + s + "\033[0m"; }

    // Combinations — bold + color, commonly used in vigil output
    inline std::string boldGreen(const std::string& s)  { return "\033[1;32m" + s + "\033[0m"; }
    inline std::string boldRed(const std::string& s)    { return "\033[1;31m" + s + "\033[0m"; }
    inline std::string boldYellow(const std::string& s) { return "\033[1;33m" + s + "\033[0m"; }
    inline std::string boldCyan(const std::string& s)   { return "\033[1;36m" + s + "\033[0m"; }
}