#include "Log.h"

#include <ncurses.h>

const int X_POS = 2;

WINDOW* logWin = nullptr;

void Log::Init() {
    logWin = newpad(LINES - 3, COLS);
    
    nodelay(logWin, true);
    scrollok(logWin, true);
    idlok(logWin, true);
    wsetscrreg(logWin, 0, LINES - 3);
    box(logWin, 0, 0);
    
    getmaxyx(stdscr, Rows, Cols);
    
    Refresh();
    
    Add("Rows: " + std::to_string(Rows) + " -  " + std::to_string(LINES));
    Add("Cols: " + std::to_string(Cols) + " -  " + std::to_string(COLS));
}

void Log::Add(std::string message) {
    mvwprintw(logWin, YPos++, X_POS, "%s\n", message.c_str());
    Refresh();
}

void Log::Refresh() {
    prefresh(logWin,
             (YPos - Rows + 2), 0,
             0, 0,
             Rows - 2, Cols);
}