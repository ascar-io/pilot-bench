/*
 * benchmark_hash.c
 * A sample program that uses libpilot to benchmark two hash functions
 *
 * Copyright (c) 2015, 2016, University of California, Santa Cruz, CA, USA.
 * Created by Yan Li <yanli@ascar.io>,
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

#include <pilot/libpilot.h>
using namespace pilot;

// We set up a 1 GB buffer. Feel free to reduce the buffer size if you can't
// allocate this much memory.
const size_t max_len = 1000 * 1000 * 1000;
char buf[max_len];
// We use global variables to store the result so the compiler would not
// optimize them away.
int hash1, hash2;

int hash_func_one(void) {
    hash1 = 1;
    for (size_t i = 0; i < max_len; ++i) {
        hash1 = 31 * hash1 + buf[i];
    }
    // 0 means correct
    return 0;
}

int hash_func_two(size_t work_amount) {
    hash2 = 1;
    for (size_t i = 0; i + 3 < work_amount; i += 4) {
        hash2 = 31 * 31 * 31 * 31 * hash2
              + 31 * 31 * 31 * buf[i]
              + 31 * 31 * buf[i + 1]
              + 31 * buf[i + 2]
              + buf[i + 3];
    }
    return 0;
}

int main() {
    // init buffer with some pseudo random data
    for (size_t i = 0; i != max_len; ++i) {
        buf[i] = (char)(i * 42);
    }
    simple_runner(hash_func_one);
    simple_runner_with_wa(hash_func_two, 1024, max_len);
}
