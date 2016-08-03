#include "TUI.h"

#include <map>
#include <mutex>
#include <thread>

#ifdef WIN32
    #include <curses.h>
#else
    #include <ncurses.h>
#endif

#include "main.h"
#include "Functions.h"

static int MaxX;
static int MaxY;

static int TopLine = 1;

static WINDOW* Top;
static WINDOW* Bottom;

static std::mutex WriteLock;
static std::thread* input;

const std::map<char, short> ColorCodes = {
    {'0', 232}, // Black
    {'1', 17 }, // Dark Blue
    {'2', 28 }, // Dark Green
    {'3', 4  }, // Dark Aqua
    {'4', 124}, // Dark Red
    {'5', 91 }, // Dark Purple
    {'6', 214}, // Gold
    {'7', 242}, // Gray
    {'8', 237}, // Dark Gray
    {'9', 21 }, // Blue
    {'a', 154}, // Green
    {'b', 123}, // Aqua
    {'c', 196}, // Red
    {'d', 213}, // Light Purple
    {'e', 226}, // Yellow
    {'f', 255}  // White
};

void Draw_Color_String(std::string str);
void Input_Thread();

void Curses::Init() {
    initscr();
    start_color();

    getmaxyx(stdscr, MaxY, MaxX);

    Top = newwin(MaxY - 3, MaxX, 0, 0);
    Bottom = newwin(3, MaxX, MaxY - 3, 0);

    scrollok(Top, TRUE);

    box(Top, '|', '=');
    box(Bottom, '|', '-');

    wsetscrreg(Top, 1, MaxY - 5);
    wsetscrreg(Bottom, 1, 1);

    for (short i = 0; i < 256; ++i) {
        init_pair(i, i, 0);
    }

    input = new std::thread(Input_Thread);
}

void Curses::Write(std::string str, bool newline) {
    WriteLock.lock();

    if (str != "") {
        Draw_Color_String(str);
    }

    if (newline) {
        if (TopLine != (MaxY - 5)) {
            ++TopLine;
        }
        else {
            scroll(Top);
        }
    }

    if (TopLine != (MaxY - 5)) {
        ++TopLine;
    }
    else {
        scroll(Top);
        box(Top, '|', '=');
    }

    wrefresh(Top);
    wmove(Bottom, 1, 2);

    WriteLock.unlock();
}

void Curses::Clean_Up() {
    endwin();
    input->join();
    delete input;
}

void Input_Thread() {
    char str[80];
    
    while (!Exit) {
		std::fill(str, str + 80, 0);

        mvwgetstr(Bottom, 1, 2, str);
        std::string message(str);

        Parse_Commands(message);

        if (message != "") {
            Curses::Write(message);
        }
        else {
            wmove(Bottom, 1, 2);
        }

        wclrtoeol(Bottom);
        wrefresh(Bottom);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Draw_Color_String(std::string str) {
    int startPos = 0;
    std::string partStr = "";
    bool checkNext = false;
    short currentColor = 7;

    mvwprintw(Top, TopLine, 3, Get_Timestamp().c_str());

    for (char const &c : str) {
        if (c == '&') {
            if (partStr.length() > 0) {
                wattron(Top, COLOR_PAIR(currentColor));
                mvwprintw(Top, TopLine, 15 + startPos, partStr.c_str());
                wattroff(Top, COLOR_PAIR(currentColor));

                startPos += static_cast<int>(partStr.length());

                partStr.clear();
            }

            checkNext = true;
            continue;
        }

        if (checkNext) {
            checkNext = false;
            currentColor = ColorCodes.at(c);
            continue;
        }

        partStr += c;
    }

    if (partStr.length() > 0) {
        wattron(Top, COLOR_PAIR(currentColor));
        mvwprintw(Top, TopLine, 15 + startPos, partStr.c_str());
        wattroff(Top, COLOR_PAIR(currentColor));
    }
}