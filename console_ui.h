#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include<cstdio>
#include<string>

#ifdef _WIN32
#include<windows.h>
#else
#include<sys/ioctl.h>
#include<unistd.h>
#endif

using namespace std;

enum Color {
    DEFAULT =39,
    BLACK   =30,
    RED     =31,
    GREEN   =32,
    YELLOW  =33,
    BLUE    =34,
    MAGENTA =35,
    CYAN    =36,
    WHITE   =37
};

enum BgColor {
    BG_DEFAULT =49,
    BG_BLACK   =40,
    BG_RED     =41,
    BG_GREEN   =42,
    BG_YELLOW  =43,
    BG_BLUE    =44,
    BG_MAGENTA =45,
    BG_CYAN    =46,
    BG_WHITE   =47
};

enum TextStyle {
    TS_NONE         = 0,
    TS_BOLD         = 1,
    TS_DIM          = 2,
    TS_ITALIC       = 3,
    TS_UNDERLINE    = 4,
    TS_BLINK        = 5,
    TS_REVERSE      = 7,
    TS_HIDDEN       = 8,
    TS_STRIKETHROUGH= 9
};

inline void mvc(int x, int y) {
    printf("\033[%d;%dH", y, x);
}

inline void clrtxt(const string& text, int txtcolor = DEFAULT, int bgcolor = BG_DEFAULT, int style = TS_NONE) {
    if (style == TS_NONE) {
        printf("\033[%d;%dm%s\033[0m", txtcolor, bgcolor, text.c_str());
    } else {
        printf("\033[%d;%d;%dm%s\033[0m", style, txtcolor, bgcolor, text.c_str());
    }
}

inline void hc() {
    printf("\033[?25l");
}

inline void sc() {
    printf("\033[?25h");
}

inline void setup() {
#ifdef _WIN32
    HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut==INVALID_HANDLE_VALUE){
        return;
    }
    DWORD dwMode=0;
    if(!GetConsoleMode(hOut,&dwMode)){
        return;
    }
    dwMode|=ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut,dwMode);
#endif
}

inline int termh() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    return 24;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 24;
#endif
}

inline int termw() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

#endif