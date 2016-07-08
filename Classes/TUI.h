#pragma once

#include <string>

namespace Curses {
    void Init();
    void Write(std::string str, bool newline = false);
    void Clean_Up();
};