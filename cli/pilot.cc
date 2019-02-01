/*
 * pilot.cc: main file for the command line pilot tool
 *
 * Copyright (c) 2017-2019 Yan Li <yanli@tuneup.ai>. All rights reserved.
 * The Pilot tool and library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License version 2.1 (not any other version) as published by the Free
 * Software Foundation.
 *
 * The Pilot tool and library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program in file lgpl-2.1.txt; if not, see
 * https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * Commit 033228934e11b3f86fb0a4932b54b2aeea5c803c and before were
 * released with the following license:
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@tuneup.ai>,
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

#include <iostream>
#include "pilot-cli.h"
#include <string>
#ifdef WITH_LUA
extern "C" {
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
  #include <prompt.h>
}
#endif

using namespace std;

void print_help_msg(const char* argv0) {
    cerr << GREETING_MSG << endl;
    cerr << "Usage: " << argv0 << " [command]" << endl;
#ifdef WITH_LUA
    cerr << "Pilot enters Lua mode if no command is given." << endl;
#endif
    cerr << "Available commands:" << endl;
    cerr << "  analyze                 analyze existing data" << endl;
    cerr << "  run_program             run a benchmark program" << endl;
    cerr << "  detect_changepoint_edm  use EDM method to detect changepoints from an input file" << endl;
    cerr << "Add --help after any command to see command specific help." << endl << endl;
    print_read_the_doc_info();
    cerr << endl;
}

int main(int argc, const char** argv) {
    PILOT_LIB_SELF_CHECK;

    // Parsing the command line arguments. First we check if a command is available.
    if (1 >= argc) {
#ifdef WITH_LUA
        // entering Lua mode if no command
        cerr << GREETING_MSG << endl;
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);               /* opens Lua basic library */
        luap_enter(L);
        return 0;
#else
        print_help_msg(argv[0]);
        return 2;
#endif
    }
    const string cmd(argv[1]);
    if ("--help" == cmd || "help" == cmd) {
        print_help_msg(argv[0]);
        return 2;
    } else if ("analyze" == cmd) {
        return handle_analyze(argc, argv);
    } else if ("run_program" == cmd) {
        return handle_run_program(argc, argv);
    } else if ("detect_changepoint_edm" == cmd) {
        return handle_detect_changepoint_edm(argc, argv);
    } else {
        cerr << "Error: Unknown command: " << cmd << endl;
        print_help_msg(argv[0]);
        return 2;
    }
}
