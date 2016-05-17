/*
 * pilot.cc: main file for the command line pilot tool
 *
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
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

#include <iostream>
#include "pilot-cli.h"
#include <string>
extern "C" {
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
  #include <prompt.h>
}

using namespace std;

void print_help_msg(const char* argv0) {
    cerr << GREETING_MSG << endl;
    cerr << "Usage: " << argv0 << " [command]" << endl;
    cerr << "Pilot enters Lua mode if no command is given." << endl;
    cerr << "Available commands:" << endl;
    cerr << "  run_program             run a benchmark program" << endl;
    cerr << "Add --help after any command to see command specific help." << endl << endl;
}

int main(int argc, const char** argv) {
    PILOT_LIB_SELF_CHECK;

    // Parsing the command line arguments. First we check if a command is available.
    if (1 >= argc) {
        // entering Lua mode if no command
        cerr << GREETING_MSG << endl;
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);               /* opens Lua basic library */
        luap_enter(L);
        return 0;
    }
    const string cmd(argv[1]);
    if ("--help" == cmd || "help" == cmd) {
        print_help_msg(argv[0]);
        return 2;
    } else if ("run_program" == cmd) {
        return handle_run_program(argc, argv);
    } else {
        cerr << "Error: Unknown command: " << cmd << endl;
        print_help_msg(argv[0]);
        return 2;
    }
}
