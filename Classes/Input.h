#pragma once

#include <string>

typedef struct _win_st WINDOW;
extern WINDOW* cmdWin;

class Log;

extern Log messageLog;

class Input {
  public:
    void Init();
    void Type(int c);
    
  private:
    int CursorPos = 2;
    std::string Message = "";
    
    void Refresh();
};