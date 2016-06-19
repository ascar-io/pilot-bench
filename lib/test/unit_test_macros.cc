/*
 * unit_test_macros.cc
 *
 * Copyright (c) 2016, University of California, Santa Cruz, CA, USA.
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

#include <libpilot.h>

const int len = 10000;
char val[len];
int h1 = 1, h2 = 1;

int hash_func_one(void) {
    for (int i = 0; i < len; i++)
        h1 = 31 * h1 + val[i];
    return 0;
}

int hash_func_two(size_t work_amount) {
    for (size_t loop = 0; loop != work_amount; ++loop)
        for (int i = 0; i + 3 < len; i += 4)
            h2 = 31 * 31 * 31 * 31 * h2
               + 31 * 31 * 31 * val[i]
               + 31 * 31 * val[i + 1]
               + 31 * val[i + 2]
               + val[i + 3];
    return 0;
}

int main() {
    // init val with some pseudo random data
    for (int i = 0; i != len; ++i)
        val[i] = (char)(i * 42);
    pilot::simple_runner(hash_func_one);
    pilot::simple_runner_with_wa(hash_func_two);
    //assert (h1 == h2);

    //pilot_comparison_t cmp_result = pilot_compare_workloads(hash_func_one, hash_func_two);
    //std::cout << cmp_result << std::endl;
}
