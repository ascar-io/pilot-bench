/*
 * libpilot.h: the main header file of libpilot. This is the only header file
 * you need to include.
 *
 * Copyright (c) 2015, University of California, Santa Cruz, CA, USA.
 *
 * Developers:
 *   Yan Li <yanli@cs.ucsc.edu>
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

#ifndef LIBPILOT_HEADER_LIBPILOT_H_
#define LIBPILOT_HEADER_LIBPILOT_H_

#include <boost/timer/timer.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <functional>
#include <vector>

extern "C" {

/**
 * \brief The type of memory allocation function, which is used by pilot_workload_func_t.
 * \details libpilot needs to ask the caller to allocate some memory and write
 * in the result readings. The caller need to use this function to allocate memory
 * that will be owned by libpilot.
 * @param size the size of memory to allocate
 */
typedef void* pilot_malloc_func_t(size_t size);
void* pilot_malloc_func(size_t size);

/**
 * \brief A function from the library's user for running one benchmark and collecting readings.
 * @param[in] total_work_amount
 * @param[out] num_of_work_unit
 * @param[out] unit_readings the reading of each work unit. Format: unit_readings[piid][unit_id]. The user needs to allocate memory using lib_malloc_func.
 * @param[out] readings the final readings of this workload run. Format: readings[piid]. The user needs to allocate memory using lib_malloc_func.
 * @return
 */
typedef int pilot_workload_func_t(size_t total_work_amount,
                                  pilot_malloc_func_t *lib_malloc_func,
                                  size_t *num_of_work_unit,
                                  double ***unit_readings,
                                  double **readings);

/**
 * \brief The data object that holds all the information of a workload.
 * \details Use pilot_new_workload to allocate an instance. The library user
 * should never be able to directly declare such a struct.
 */
struct pilot_workload_t;

pilot_workload_t* pilot_new_workload(const char *workload_name);
void pilot_set_num_of_pi(pilot_workload_t*, size_t num_of_readings);
void pilot_set_workload_func(pilot_workload_t*, pilot_workload_func_t*);
void pilot_set_total_work_amount(pilot_workload_t*, size_t);

/**
 * \brief Run the workload as specified in wl
 * @param[in] wl pointer to the workload struct
 * @return 0 on success, otherwise error code. On error, call pilot_strerror to
 * get a pointer to the error message.
 */
int pilot_run_workload(pilot_workload_t *wl);

/**
 * \brief Get pointer to error message string
 * @param errnum the error number returned by a libpilot function
 * @return a pointer to a static memory of error message
 */
const char *pilot_strerror(int errnum);

/**
 * \brief Export the session data into a csv file
 * @param[in] wl pointer to the workload struct
 * @param file_name the file name for export
 * @return 0 on success, otherwise error code. On error, call pilot_strerror to
 * get a pointer to the error message.
 */
int pilot_export_session_data(pilot_workload_t *wl, const char *file_name);

/**
 * \brief Estimate the sample variance when sample cannot be proven not
 * correlated and the distribution of sample mean is unknown.
 * \details This function uses the method of independent replications as
 * described in equation (2.18) and (2.19) in [Ferrari78].
 * @param n sample size
 * @param sample sample data
 * @param q size of independent subsessions
 * @return
 */
int pilot_est_sample_var_dist_unknown(const size_t n, const double *sample, size_t q);

/**
 * \brief Return the total number of rounds so far.
 * @param[in] wl pointer to the workload struct
 * @return the number of rounds; a negative number on error
 */
int pilot_get_num_of_rounds(const pilot_workload_t *wl);

/**
 * \brief Return the read only copy of all readings of a performance index
 * @param[in] wl pointer to the workload struct
 * @param piid Performance Index ID
 * @return a pointer to readings data, the length of which can be get by using pilot_get_num_of_rounds(); NULL on error.
 */
const double* pilot_get_pi_readings(const pilot_workload_t *wl, size_t piid);

/**
 * \brief Return the read only copy of all unit readings of a performance index in a certain round
 * @param[in] wl pointer to the workload struct
 * @param piid Performance Index ID
 * @param round Performance Index ID
 * @param[out] num_of_work_units the number of work units in that round
 * @return the data of all unit readings of PIID in that round. It is a read-only array of size num_of_work_units.
 */
const double* pilot_get_pi_unit_readings(const pilot_workload_t *wl, size_t piid, size_t round, size_t *num_of_work_units);

int pilot_destroy_workload(pilot_workload_t *wl);

int pilot_calculate_optimal_length();

}

#endif /* LIBPILOT_HEADER_LIBPILOT_H_ */
