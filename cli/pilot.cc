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
#include <cassert>
#include <cdk.h>
#include <string>
#include <vector>

using namespace std;

#define QUIT            CTRL('Q')
#define ESCAPE          CTRL('[')

const chtype DEFAULT_COLOR = COLOR_PAIR(5);

/**
 * \brief This is a model class for tasks information as in the MVC architecture
 */
class TaskList {
private:
    vector<char*> tasks_;
public:
    TaskList() {
        tasks_.push_back(strdup("Task1"));
        tasks_.push_back(strdup("Task2"));
        tasks_.push_back(strdup("Task3"));
        tasks_.push_back(strdup("Task4"));
        tasks_.push_back(strdup("Task5"));
        tasks_.push_back(strdup("Task6"));
    }
    ~TaskList() {
        for (auto s : tasks_)
            free(s);
    }
    char** data(void) {
        return tasks_.data();
    }
    size_t size(void) {
        return tasks_.size();
    }
};

class BoxedWidget {
private:
    int x_;      //! the X of the box
    int y_;      //! the Y of the box
    int w_;      //! the width of the box
    int h_;      //! the height of the box
    WINDOW    *wind_;

    void draw(void) {
        wind_ = newwin (h_, w_, y_, x_);
        cdk_screen_ = initCDKScreen(wind_);
        wbkgd(wind_, DEFAULT_COLOR | A_BOLD);
        box (wind_, ACS_VLINE, ACS_HLINE);
        //wrefresh (wind_);
        writeChar(cdk_screen_->window,
                  (w_ - title_.size()) / 2,
                  0,
                  const_cast<char*>(title_.c_str()),
                  HORIZONTAL, 0,
                  title_.size());
        //wattrset(wind_, dialog_attr);
    }
protected:
    CDKSCREEN *cdk_screen_;
    int inner_w_; //! the width of the space for the inner widget
    int inner_h_; //! the height of the space for the inner widget
public:
    //int y_cur_;
    //int y_end_;
    const string title_;

    virtual void refresh(void) {
        //wrefresh(wind_);
        refreshCDKScreen(cdk_screen_);
    }
//    void scroll_up(void);
//    void scroll_down(void);
//    void scroll_page_up(void);
//    void scroll_page_down(void);
//    void scroll_home(void);
//    void scroll_end(void);

    BoxedWidget(int h, int w, int y, int x, const string &title)
    : x_(x), y_(y), w_(w), h_(h), title_(title), wind_(NULL) {
        inner_w_ = w_ - 3;
        inner_h_ = h_ - 2;
        draw();
    }
    virtual ~BoxedWidget() {
        //eraseCursesWindow(wind_);
        if (cdk_screen_) destroyCDKScreen(cdk_screen_);
        if (wind_) delwin(wind_);
    }
};

class TaskBox {
private:
    int x_;      //! the X of the box
    int y_;      //! the Y of the box
    int w_;      //! the width of the box
    int h_;      //! the height of the box
    CDKSCREEN *cdk_screen_;
    CDKSCROLL *scroll_list_;
    CDKLABEL  *title_label_;
    vector<char*> items_;
    TaskList *task_list_;
    const char *title_[1] = {" TASK LIST "};

    void draw(void) {
        scroll_list_ = newCDKScroll(cdk_screen_,
                                    x_,
                                    y_,
                                    RIGHT,
                                    h_,
                                    w_,
                                    NULL,
                                    task_list_->data(),
                                    task_list_->size(),
                                    FALSE,
                                    A_REVERSE,
                                    TRUE,
                                    FALSE);
        setCDKScrollBackgroundAttrib(scroll_list_, DEFAULT_COLOR | A_BOLD);
        title_label_ = newCDKLabel(cdk_screen_, x_ + (w_ - strlen(title_[0])) / 2, y_,
                                   const_cast<char**>(title_), 1, FALSE, FALSE);
        setCDKLabelBackgroundAttrib(title_label_, DEFAULT_COLOR | A_BOLD);
    }
public:
    TaskBox(CDKSCREEN *cdk_screen, int h, int w, int y, int x, TaskList *task_list)
    : cdk_screen_(cdk_screen), x_(x), y_(y), w_(w), h_(h),
      task_list_(task_list) {
        draw();
    }

    ~TaskBox() {
        if (scroll_list_) destroyCDKScroll(scroll_list_);
        if (title_label_) destroyCDKLabel(title_label_);
    }
};

class SummaryBox : public BoxedWidget {
private:
    void draw(void) {
    }
public:
    SummaryBox(int h, int w, int y, int x)
    : BoxedWidget(h, w, y, x, " WORKLOAD SUMMARY ") {
        draw();
    }
    ~SummaryBox() {
    }
};

class MsgBox {
private:
    int x_;      //! the X of the box
    int y_;      //! the Y of the box
    int w_;      //! the width of the box
    int h_;      //! the height of the box
    CDKSCREEN  *cdk_screen_;
    CDKSWINDOW *swindow_;
    CDKLABEL   *title_label_;
    const char *title_[1] = {" MESSAGES "};

    void draw(void) {
        swindow_ = newCDKSwindow(cdk_screen_,
                                 x_,
                                 y_,
                                 h_,
                                 w_,
                                 NULL,
                                 500,
                                 TRUE,
                                 FALSE);
        setCDKSwindowBackgroundAttrib(swindow_, DEFAULT_COLOR + 14 | A_BOLD);
        title_label_ = newCDKLabel(cdk_screen_, x_ + (w_ - strlen(title_[0])) / 2, y_,
                                   const_cast<char**>(title_), 1, FALSE, FALSE);
        setCDKLabelBackgroundAttrib(title_label_, DEFAULT_COLOR | A_BOLD);
    }
public:
    MsgBox(CDKSCREEN *cdk_screen, int h, int w, int y, int x)
    : cdk_screen_(cdk_screen), x_(x), y_(y), w_(w), h_(h - 1) {
        draw();
    }
    ~MsgBox() {
        if (swindow_) destroyCDKSwindow(swindow_);
    }
    void add_msg(const string &msg) {
        assert (swindow_);
        addCDKSwindow (swindow_, msg.c_str(), BOTTOM);
    }
};

/**
 * \brief This is the view class as in the MVC architecture
 */
class PilotTUI {
private:
    TaskList   *task_list_;

    WINDOW     *curses_win_;
    CDKSCREEN  *cdk_screen_;

    CDKLABEL   *title_bar_;
    CDKLABEL   *progress_bar_;
    CDKLABEL   *bottom_bar_a_;
    CDKLABEL   *bottom_bar_b_;
    TaskBox    *task_box_;
    SummaryBox *summary_box_;
    MsgBox     *msg_box_;
    const int  left_col_width_;
    const int  task_box_height_;
    const char *_title[1] = {"</57> PILOT v0.2 <#VL> "};
    const char *bottom_txt_[1] = {"</57> <!57></9>Q<!9></57> Quit<!57>"};
    int         bottom_txt_len_ = 7;
public:
    PilotTUI(TaskList *task_list) : task_list_(task_list),
        left_col_width_(30), task_box_height_(task_list->size() + 2),
        title_bar_(NULL), progress_bar_(NULL), task_box_(NULL),
        summary_box_(NULL), msg_box_(NULL) {
        curses_win_ = initscr();
        // Hide the cursor
        curs_set(0);
        cdk_screen_ = initCDKScreen(curses_win_);
        initCDKColor();
        draw();
        refresh();
    }
    ~PilotTUI() {
        delete msg_box_;
        delete summary_box_;
        delete task_box_;
        destroyCDKLabel(bottom_bar_b_);
        destroyCDKLabel(bottom_bar_a_);
        destroyCDKLabel(title_bar_);
        destroyCDKScreen(cdk_screen_);
        endCDK();
    }
    void draw() {
        if (title_bar_) {
            // this is a redraw
            destroyCDKLabel(title_bar_);
            destroyCDKLabel(bottom_bar_b_);
            destroyCDKLabel(bottom_bar_a_);
            delete msg_box_;
            delete summary_box_;
            delete task_box_;
        }
        title_bar_ = newCDKLabel(cdk_screen_, 0, 0,
                                 const_cast<char**>(_title), 1, FALSE, FALSE);
        // draw the first half the bottom bar
        bottom_bar_a_ = newCDKLabel(cdk_screen_, 0, LINES - 1,
                                    const_cast<char**>(bottom_txt_), 1, FALSE, FALSE);
        // draw the second half the bottom bar, which is empty that fills the rest of the line
        char *buf[1];
        int second_half_len = COLS - bottom_txt_len_ + 1;
        buf[0] = new char[second_half_len];
        memset(buf[0], ' ', second_half_len - 1);
        buf[0][second_half_len - 1] = 0;
        bottom_bar_b_ = newCDKLabel(cdk_screen_, bottom_txt_len_, LINES - 1, buf, 1, FALSE, FALSE);
        setCDKLabelBackgroundAttrib(bottom_bar_b_, COLOR_PAIR(57));
        delete[] buf[0];

        task_box_ = new TaskBox(cdk_screen_,
                                task_box_height_,
                                left_col_width_,
                                1,
                                0,
                                task_list_);
        summary_box_ = new SummaryBox(LINES - task_box_height_ - 2,
                                      left_col_width_,
                                      task_box_height_ + 1,
                                      0);
        msg_box_ = new MsgBox(cdk_screen_, LINES - 2, COLS - left_col_width_, 1, left_col_width_);
    }
    void refresh() {
        refreshCDKScreen(cdk_screen_);
        summary_box_->refresh();
    }
    void event_loop() {
        int ch = ERR;
        do {
            switch (ch) {
//            case KEY_HOME:
//                msg_BoxedWidget_->scroll_home();
//                break;
//            case KEY_END:
//                msg_BoxedWidget_->scroll_end();
//                break;
//            case KEY_PREVIOUS:
//                case KEY_PPAGE:
//                msg_BoxedWidget_->scroll_page_up();
//                break;
//            case KEY_NEXT:
//                case KEY_NPAGE:
//                msg_BoxedWidget_->scroll_page_down();
//                break;
//            case CTRL('P'):
//                case KEY_UP:
//                msg_BoxedWidget_->scroll_up();
//                break;
//            case CTRL('N'):
//                case KEY_DOWN:
//                msg_BoxedWidget_->scroll_down();
//                break;
            default:
                msg_box_->add_msg("a key is pressed\na key is pressed");
                beep();
                break;
            case ERR:
                break;
            }
            // no need to refresh every time
        } while ((ch = getch()) != ERR && ch != QUIT && ch != ESCAPE && ch != 'Q' && ch != 'q');
        printf ("%d\n", ch);
    }
};

int main(int argc, char** argv) {
    TaskList task_list;
    PilotTUI pilot_tui(&task_list);
    getchar();
    pilot_tui.event_loop();
    return 0;
}

//void BoxedWidget::redraw(void) {
//    wbkgdset(wind_, menubox_attr | ' ');
//    //wclrtobot(wind_);
//    //refresh();
//    wrefresh(wind_);
//    //wsyncup(wind_);
//    //wcursyncup(wind_);
//    dlg_trace_win(wind_);
//}
//
//void BoxedWidget::scroll_up(void) {
//    if (y_cur_ > 0)
//        --y_cur_;
//    else
//        beep();
//}
//
//void BoxedWidget::scroll_down(void) {
//    if (y_cur_ < y_end_)
//        ++y_cur_;
//    else
//        beep();
//}
//
//void BoxedWidget::scroll_page_up(void) {
//    if (y_cur_ > 0) {
//        y_cur_ -= height_ / 2;
//        y_cur_ = max(0, y_cur_);
//    } else
//        beep();
//}
//
//void BoxedWidget::scroll_page_down(void) {
//    if (y_cur_ < y_end_) {
//        y_cur_ += height_ / 2;
//        y_cur_ = min(y_cur_, y_end_);
//    } else
//        beep();
//}
//
//void BoxedWidget::scroll_home(void) {
//    y_cur_ = 0;
//}
//
//void BoxedWidget::scroll_end(void) {
//    y_cur_ = y_end_;
//}
