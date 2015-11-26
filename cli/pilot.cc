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
#include <cdk.h>
#include <string>

using namespace std;

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
    const string title_;

    void redraw(void);
    void scroll_up(void);
    void scroll_down(void);
    void scroll_page_up(void);
    void scroll_page_down(void);
    void scroll_home(void);
    void scroll_end(void);
    Window(int height, int width, int y, int x, const string &title)
    : x_(x), y_(y), width_(width), height_(height),
            y_cur_(0), title_(title) {
        static DLG_KEYS_BINDING binding[] = {
                HELPKEY_BINDINGS,
                ENTERKEY_BINDINGS,
                NAVIGATE_BINDINGS,
                END_KEYS_BINDING
        };

        wind_ = dlg_new_window(height_, width_, y_, x_);
        dlg_register_window(wind_, title_.c_str(), binding);
        //dlg_register_buttons(dialog, "formbox", buttons);
        dlg_draw_box2(wind_, 0, 0, height_, width_, dialog_attr, border_attr, border2_attr);
        //dlg_draw_bottom_box2(wind_, border_attr, border2_attr, dialog_attr);
        dlg_draw_title(wind_, title_.c_str());
        wattrset(wind_, dialog_attr);
    }
    ~Window() {
        dlg_del_window(wind_);
    }
};

class TaskWindow : public Window {
public:
    TaskWindow(int height, int width, int y, int x)
    : Window(height, width, y, x, "Task") {

    }
};

class SummaryWindow : public Window {
    int x_;
    int y_;
    int width_;
    int height_;
    int y1_ = 0;
    int y2_ = 0;

public:
    SummaryWindow(int height, int width, int y, int x)
    : Window(height, width, y, x, "Summary") {

    }
};

class MsgWindow : public Window {

public:
    MsgWindow(int height, int width, int y, int x)
    : Window(height, width, y, x, "Messages") {
    }
    void add_msg(const char *msg);
};

class PilotTUI {
private:
    unique_ptr<TaskWindow>    task_window_;
    unique_ptr<SummaryWindow> summary_window_;
    unique_ptr<MsgWindow>     msg_window_;
    const int left_col_width_;
    const int task_window_height_;

public:
    PilotTUI() : left_col_width_(30), task_window_height_(30) {
  cursesWin = initscr();
  cdkscreen = initCDKScreen(cursesWin);
  // Now, set up color.
  initCDKColor();
        init_dialog(stdin, stdout);
        task_window_.reset(new TaskWindow(task_window_height_, left_col_width_, 1, 0));
        summary_window_.reset(new SummaryWindow(LINES - task_window_height_ - 2, left_col_width_, task_window_height_ + 1, 0));
        msg_window_.reset(new MsgWindow(LINES - 2, COLS - left_col_width_, 1, left_col_width_));
    }
    ~PilotTUI() {
        end_dialog();
    }
    void redraw() {
        task_window_->redraw();
        summary_window_->redraw();
        msg_window_->redraw();
    }
    void event_loop() {
        int ch = ERR;
        do {
            switch (ch) {
            case KEY_HOME:
                msg_window_->scroll_home();
                break;
            case KEY_END:
                msg_window_->scroll_end();
                break;
            case KEY_PREVIOUS:
                case KEY_PPAGE:
                msg_window_->scroll_page_up();
                break;
            case KEY_NEXT:
                case KEY_NPAGE:
                msg_window_->scroll_page_down();
                break;
            case CTRL('P'):
                case KEY_UP:
                msg_window_->scroll_up();
                break;
            case CTRL('N'):
                case KEY_DOWN:
                msg_window_->scroll_down();
                break;
            default:
                beep();
                break;
            case ERR:
                break;
            }
            redraw();
        } while ((ch = getch()) != ERR && ch != QUIT && ch != ESCAPE && ch != 'Q' && ch != 'q');
    }
};

int main(int argc, char** argv) {
    PilotTUI pilot_tui;

    pilot_tui.event_loop();
    return 0;
}

void Window::redraw(void) {
    wbkgdset(wind_, menubox_attr | ' ');
    //wclrtobot(wind_);
    //refresh();
    wrefresh(wind_);
    //wsyncup(wind_);
    //wcursyncup(wind_);
    dlg_trace_win(wind_);
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
