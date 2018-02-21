/*
 * python_binding.cc
 *
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

#include <patchlevel.h>
#include <boost/python.hpp>
#include "config.h"
#include <exception>
#include <libpilotcpp.h>

using namespace boost::python;
using namespace pilot;
using namespace std;

// boost::python::stl_input_iterator<> is broken, see
// https://github.com/boostorg/python/issues/69 , so we have to write our own
// iterator. This is a simple and inefficient implementation and needs to be
// fixed.
template <typename T>
class PythonListIterator {
public:
    PythonListIterator(const list &o) : ob_(&o), index_(0) {}
    PythonListIterator(const PythonListIterator &a) : ob_(a.ob_), index_(a.index_) {}
    T operator*() const { return extract<T>((*ob_)[index_])(); }
    PythonListIterator& operator++() { ++index_; return *this; }
    PythonListIterator operator++(int) { PythonListIterator r(*this); ++index_; return r; }
private:
    const list *ob_;
    size_t index_;
};

double pilot_subsession_mean_python(list data, pilot_mean_method_t mean_method) {
    return pilot_subsession_mean(PythonListIterator<double>(data), len(data), mean_method);
}

double pilot_subsession_autocorrelation_coefficient_python(list data, int q, double sample_mean, pilot_mean_method_t mean_method) {
    if (mean_method != ARITHMETIC_MEAN && mean_method != HARMONIC_MEAN) {
        throw runtime_error("Invalid value for mean_method, must be ARITHMETIC_MEAN or HARMONIC_MEAN");
    }
    return pilot_subsession_autocorrelation_coefficient(
            PythonListIterator<double>(data), len(data), q,
            sample_mean, mean_method);
}

double pilot_subsession_confidence_interval_python(list data, int q, double confidence_level, pilot_mean_method_t mean_method) {
    if (mean_method != ARITHMETIC_MEAN && mean_method != HARMONIC_MEAN) {
        throw runtime_error("Invalid value for mean_method, must be ARITHMETIC_MEAN or HARMONIC_MEAN");
    }
    return pilot_subsession_confidence_interval(
            PythonListIterator<double>(data), len(data), q,
            confidence_level, mean_method);
}


BOOST_PYTHON_MODULE(pilot_bench)
{
    PILOT_LIB_SELF_CHECK;
    pilot_set_log_level(lv_info);

    enum_<pilot_mean_method_t>("pilot_mean_method")
        .value("ARITHMETIC_MEAN", ARITHMETIC_MEAN)
        .value("HARMONIC_MEAN", HARMONIC_MEAN)
        .export_values()
        ;

    def("pilot_subsession_mean", pilot_subsession_mean_python);
    def("pilot_subsession_autocorrelation_coefficient", pilot_subsession_autocorrelation_coefficient_python);
    def("pilot_subsession_confidence_interval", pilot_subsession_confidence_interval_python);
}
