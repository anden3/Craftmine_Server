#include "Input.h"

#include <ncurses.h>

#include "Log.h"

const int X_START = 2;
const int Y_POS = 1;

const int ENTER = 10;
const int BACKSPACE = 127;

WINDOW* cmdWin = nullptr;

void Input::Init() {
    cmdWin = newwin(3, COLS, LINES - 3, 0);
    nodelay(cmdWin, true);
    box(cmdWin, 0, 0);
    
    wmove(cmdWin, Y_POS, CursorPos--);
    
    Refresh();
}

void Input::Type(int c) {
    if (c == ENTER) {
        if (Message.length() > 0) {
            std::string empty = " ";
            
            for (int i = 0; i < Message.length(); ++i) {
                empty += " ";
            }
            
            CursorPos = X_START;
            mvwaddstr(cmdWin, Y_POS, CursorPos, empty.c_str());
            wmove(cmdWin, Y_POS, CursorPos--);
            
            messageLog.Add(Message);
            
            Message.clear();
        }
    }
    
    else if (c == BACKSPACE) {
        if (Message.length() > 0) {
            Message.pop_back();
            mvwaddch(cmdWin, Y_POS, CursorPos, ' ');
            wmove(cmdWin, Y_POS, CursorPos--);
        }
    }
    
    else {
        Message += c;
        mvwaddch(cmdWin, Y_POS, ++CursorPos, c);
        wmove(cmdWin, Y_POS, CursorPos + 1);
    }
    
    Refresh();
}

void Input::Refresh() {
    wrefresh(cmdWin);
}