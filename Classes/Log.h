#pragma once

#include <string>

typedef struct _win_st WINDOW;
extern WINDOW* logWin;

class Log {
  public:
    void Init();
    void Add(std::string message);
    
  private:
    int YPos = 1;
    int Rows, Cols;
    
    void Refresh();
};