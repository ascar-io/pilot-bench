/*
 * pilot.cc: main file for the command line pilot tool
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@ucsc.edu, elliot.li.tech@gmail.com>,
 * Department of Computer Science, Baskin School of Engineering.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Storage Systems Research Center, the
 *       University of California, nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include "config.h"
#include <form.h>

using namespace std;

#define CTRL(x) ((x) & 0x1f)
#define QUIT            CTRL('Q')
#define ESCAPE          CTRL('[')

class Window {
public:
    int x_;
    int y_;
    int width_;
    int height_;
    int y_cur_;
    int y_end_;
    WINDOW *wind_;
    WINDOW *pad_;

    void update(void);
    void scroll_up(void);
    void scroll_down(void);
    void scroll_page_up(void);
    void scroll_page_down(void);
    void scroll_home(void);
    void scroll_end(void);
    Window(int x, int y, int w, int h) : x_(x), y_(y), width_(w), height_(h),
            y_cur_(0) {
        wind_ = newwin(height_, width_, y_, x_);
        pad_ = newpad(height_ - 2, width_ - 2);

        //keypad(wind_, TRUE);
        //keypad(pad_, TRUE);

        waddstr(pad_, "Defined form edit/traversal keys:\n");
        for (int n = 0; n < 5; ++n) {
            wprintw(pad_, "Line %d\n", n);
        }
        waddstr(pad_, "Arrow keys move within a field as you would expect.");
        y_end_ = getcury(pad_);
    }
    ~Window() {
        werase(wind_);
        wrefresh(wind_);
        delwin(wind_);
        delwin(pad_);
    }
};

class SummaryWindow {
    int x_;
    int y_;
    int width_;
    int height_;
    int y1_ = 0;
    int y2_ = 0;

    SummaryWindow (int x, int y) : x_(x), y_(y), width_(30), y1_(0), y2_(0),
            height_(LINES - y_ - 2) {
    }
};

int main(int argc, char** argv) {
    initscr();
    cbreak();
    noecho();
    raw();
    nonl();			/* lets us read ^M's */
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_CYAN, COLOR_BLACK);
        bkgd((chtype) COLOR_PAIR(1));
        refresh();
    }

    Window task_window(0, 0, 30, 15);
    Window summary_window(0, 14, 30, 15);

    int ch = ERR;
    do {
        switch (ch) {
        case KEY_HOME:
            task_window.scroll_home();
            break;
        case KEY_END:
            task_window.scroll_end();
            break;
        case KEY_PREVIOUS:
        case KEY_PPAGE:
            task_window.scroll_page_up();
            break;
        case KEY_NEXT:
        case KEY_NPAGE:
            task_window.scroll_page_down();
            break;
        case CTRL('P'):
        case KEY_UP:
            task_window.scroll_up();
            break;
        case CTRL('N'):
        case KEY_DOWN:
            task_window.scroll_down();
            break;
        default:
            beep();
            break;
        case ERR:
            break;
        }
        task_window.update();
        summary_window.update();
    } while ((ch = getch()) != ERR && ch != QUIT && ch != ESCAPE);

    endwin();
    return 0;
}

void Window::update(void) {
    werase(wind_);
    box(wind_, 0, 0);
    wnoutrefresh(wind_);
    pnoutrefresh(pad_, y_cur_, 0, y_ + 1, x_ + 1, height_, width_);
    doupdate();
}

void Window::scroll_up(void) {
    if (y_cur_ > 0)
        --y_cur_;
    else
        beep();
}

void Window::scroll_down(void) {
    if (y_cur_ < y_end_)
        ++y_cur_;
    else
        beep();
}

void Window::scroll_page_up(void) {
    if (y_cur_ > 0) {
        y_cur_ -= height_ / 2;
        y_cur_ = max(0, y_cur_);
    } else
        beep();
}

void Window::scroll_page_down(void) {
    if (y_cur_ < y_end_) {
        y_cur_ += height_ / 2;
        y_cur_ = min(y_cur_, y_end_);
    } else
        beep();
}

void Window::scroll_home(void) {
    y_cur_ = 0;
}

void Window::scroll_end(void) {
    y_cur_ = y_end_;
}
