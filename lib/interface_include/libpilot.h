/*
 * libpilot.h: the main header file of libpilot. This is the only header file
 * you need to include.
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

#ifndef LIBPILOT_HEADER_LIBPILOT_H_
#define LIBPILOT_HEADER_LIBPILOT_H_

// This file must retain C99-compatibility so do not include C++ header files
// here.
#include "config.h"
#include <fcntl.h>
#include "pilot_exports.h"
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum pilot_error_t {
    NO_ERROR = 0,
    ERR_WRONG_PARAM = 2,
    ERR_NOMEM = 3,
    ERR_IO = 5,
    ERR_UNKNOWN_HOOK = 6,
    ERR_NOT_INIT = 11,
    ERR_WL_FAIL = 12,
    ERR_NO_READING = 13,
    ERR_STOPPED_BY_HOOK = 14,
    ERR_NOT_IMPL = 200,
    ERR_LINKED_WRONG_VER = 201
};

typedef int_least64_t nanosecond_type;

/**
 * \brief Performance libpilot self check and initialization
 * \details Include this macro at the beginning of your program.
 */
#define PILOT_LIB_SELF_CHECK pilot_lib_self_check(PILOT_VERSION_MAJOR, \
        PILOT_VERSION_MINOR, sizeof(nanosecond_type))

void pilot_lib_self_check(int vmajor, int vminor, size_t nanosecond_type_size);

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

struct pilot_pi_unit_readings_iter_t;

/**
 * \brief the function that calculates how many samples are needed for getting
 * the desired result (used mainly in test cases)
 * \details this function calculates the number of samples needed from the
 * existing readings and the desired confidence interval and confidence
 * level as specified in wl
 * @param[in] wl pointer to the workload struct
 * @param piid the Performance Index to calculate
 * @return
 */
typedef ssize_t calc_readings_required_func_t(const pilot_workload_t* wl, int piid);

/**
 * \brief The default function for calculating how many unit readings are needed to
 * be get a statistically sound result
 * @param[in] wl pointer to the workload struct
 * @return the number of readings needed; a negative number if there is not
 * enough data to calculate the required size
 */
ssize_t default_calc_unit_readings_required_func(const pilot_workload_t* wl, int piid);

/**
 * \brief The default function for calculating how many readings are needed to
 * be get a statistically sound result
 * @param[in] wl pointer to the workload struct
 * @return the number of readings needed
 */
ssize_t default_calc_readings_required_func(const pilot_workload_t* wl, int piid);

/**
 * \brief Set the function hook that calculates how many readings are needed
 * for a session (used mainly in test cases)
 * @param[in] wl pointer to the workload struct
 * @param f the new hook function
 */
void pilot_set_calc_readings_required_func(pilot_workload_t* wl, calc_readings_required_func_t *f);

/**
 * \brief Set the function hook that calculates how many unit readings are needed
 * for a session (used mainly in test cases)
 * @param[in] wl pointer to the workload struct
 * @param f the new hook function
 */
void pilot_set_calc_unit_readings_required_func(pilot_workload_t* wl, calc_readings_required_func_t *f);

/**
 * \brief Type for the general hook functions
 * @param[in] wl pointer to the workload struct
 * @return return false to stop the execution of the ongoing libpilot process
 */
typedef bool general_hook_func_t(pilot_workload_t* wl);

enum pilot_hook_t {
    PRE_WORKLOAD_RUN,
    POST_WORKLOAD_RUN
};

/**
 * \brief Set a hook function
 * @param[in] wl pointer to the workload struct
 * @param hook the hook to change
 * @param f the new hook function
  * @return 0 on success; otherwise error code
 */
int pilot_set_hook_func(pilot_workload_t* wl, enum pilot_hook_t hook, general_hook_func_t *f);

/**
 * \brief Type for a function that formats a unit reading for output
 * \details libpilots calls this function whenever it needs to format a unit
 * reading for display. You can set a different function for each PI.
 * @param[in] wl pointer to the workload struct
 * @param unit_reading the unit reading to display
 * @return a processed number for print
 */
typedef double pilot_pi_unit_reading_format_func_t(const pilot_workload_t* wl, double unit_reading);
double pilot_default_pi_unit_reading_format_func(const pilot_workload_t* wl, double unit_reading);

/**
 * \brief Set the function for formatting a unit reading for print
 * @param[in] wl pointer to the workload struct
 * @param piid the piid of which to change the format function
 * @param f the new function, omit this parameter to set it back to the default function
 * @param[in] unit a NULL-terminated string of the unit of the PI
 */
void pilot_set_pi_unit_reading_format_func(pilot_workload_t* wl, int piid,
        pilot_pi_unit_reading_format_func_t *f = &pilot_default_pi_unit_reading_format_func,
        const char *unit = NULL);

/**
 * \brief Type for a function that formats a reading for output
 * \details libpilots calls this function whenever it needs to format a
 * reading for display. You can set a different function for each PI.
 * @param[in] wl pointer to the workload struct
 * @param reading the unit reading to display
 * @return a processed number for print
 */
typedef double pilot_pi_reading_format_func_t(const pilot_workload_t* wl, double reading);
double pilot_default_pi_reading_format_func(const pilot_workload_t* wl, double reading);

/**
 * \brief Set the function for formatting a reading for print
 * @param[in] wl pointer to the workload struct
 * @param piid the piid of which to change the format function
 * @param f the new function, omit this parameter to set it back to the default function
 * @param[in] unit a NULL-terminated string of the unit of the PI
 */
void pilot_set_pi_reading_format_func(pilot_workload_t* wl, int piid,
        pilot_pi_reading_format_func_t *f = &pilot_default_pi_reading_format_func,
        const char *unit = NULL);

pilot_workload_t* pilot_new_workload(const char *workload_name);

/**
 * \brief Set the number of performance indices to record
 * @param[in] wl pointer to the workload struct
 * @param num_of_pi the number of performance indices
 */
void pilot_set_num_of_pi(pilot_workload_t* wl, size_t num_of_pi);

/**
 * \brief Get the number of performance indices
 * @param[in] wl pointer to the workload struct
 * @param[out] p_num_of_pi the pointer for storing the num_of_pi
 * @return 0 on success; aborts if wl is NULL; otherwise error code
 */
int pilot_get_num_of_pi(const pilot_workload_t* wl, size_t *p_num_of_pi);

void pilot_set_workload_func(pilot_workload_t*, pilot_workload_func_t*);

/**
 * \brief Set the upper limit for work amount that pilot should attempt
 * \details pilot will start with this init_work_amount and repeat the workload
 * and/or increase the work amount until the required confidence level
 * can be achieved, but it will never do more than work_amount_limit
 * in a single round.
 * @param[in] wl pointer to the workload struct
 * @param init_work_amount the initial work amount
 */
void pilot_set_work_amount_limit(pilot_workload_t* wl, size_t work_amount_limit);

/**
 * \brief Get work_amount_limit, the upper limit of work amount that pilot should attempt
 * @param[in] wl pointer to the workload struct
 * @param[out] p_work_amount_limit the pointer for storing the work_amount_limit
 * @return 0 on success; aborts if wl is NULL; otherwise error code
 */
int pilot_get_work_amount_limit(const pilot_workload_t* wl, size_t *p_work_amount_limit);

/**
 * \brief Set the initial work amount that pilot should attempt
 * \details pilot will start with this init_work_amount and repeat the workload
 * and/or increase the work amount until the required confidence level
 * can be achieved, but it will never do more than work_amount_limit
 * in a single round.
 * @param[in] wl pointer to the workload struct
 * @param init_work_amount the initial work amount
 */
void pilot_set_init_work_amount(pilot_workload_t* wl, size_t init_work_amount);

/**
 * \brief Get init_work_amount, the initial work amount that pilot should attempt
 * @param[in] wl pointer to the workload struct
 * @param[out] p_init_work_amount the pointer for storing init_work_amount
 * @return 0 on success; aborts if wl is NULL; otherwise error code
 */
int pilot_get_init_work_amount(const pilot_workload_t* wl, size_t *p_init_work_amount);

enum pilot_warm_up_removal_detection_method_t {
    NO_WARM_UP_REMOVAL = 0,
    FIXED_PERCENTAGE,
    MOVING_AVERAGE,
};

/**
 * \brief Set the warm-up removal method
 * @param[in] wl pointer to the workload struct
 * @param m the warm-up removal method
 */
void pilot_set_warm_up_removal_method(pilot_workload_t* wl, pilot_warm_up_removal_detection_method_t m);

/**
 * \brief Detect the ending location of the warm-up phase
 * @param[in] readings input data (readings)
 * @param num_of_readings size of input data
 * @param method the detection method
 * @return location of the end of the warm-up phase; negative value on detection failure
 */
ssize_t pilot_warm_up_removal_detect(const double *readings,
                                     size_t num_of_readings,
                                     pilot_warm_up_removal_detection_method_t method);


/**
 * \brief Whether to check for very short-lived workload
 * @param[in] wl pointer to the workload struct
 * @param check_short_workload true to enable short-lived workload check
 */
void pilot_set_short_workload_check(pilot_workload_t* wl, bool check_short_workload);

/**
 * \brief Run the workload as specified in wl
 * @param[in] wl pointer to the workload struct
 * @return 0 on success, otherwise error code. On error, call pilot_strerror to
 * get a pointer to the error message.
 */
int pilot_run_workload(pilot_workload_t *wl);

/**
 * \brief Get the number of total valid unit readings after warm-up removal
 * @param[in] wl pointer to the workload struct
 * @return the total number of valid unit readings
 */
size_t pilot_get_total_num_of_unit_readings(const pilot_workload_t *wl, int piid);

/**
 * \brief Get pointer to error message string
 * @param errnum the error number returned by a libpilot function
 * @return a pointer to a static memory of error message
 */
const char *pilot_strerror(int errnum);

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
 * \brief Return the read only copy of all raw readings of a performance index
 * @param[in] wl pointer to the workload struct
 * @param piid Performance Index ID
 * @return a pointer to readings data, the length of which can be get by using pilot_get_num_of_rounds(); NULL on error.
 */
const double* pilot_get_pi_readings(const pilot_workload_t *wl, size_t piid);

/**
 * \brief Return the read only copy of all raw unit readings of a performance index in a certain round
 * @param[in] wl pointer to the workload struct
 * @param piid Performance Index ID
 * @param round Performance Index ID
 * @param[out] num_of_work_units the number of work units in that round
 * @return the data of all unit readings of PIID in that round. It is a read-only array of size num_of_work_units.
 */
const double* pilot_get_pi_unit_readings(const pilot_workload_t *wl, size_t piid, size_t round, size_t *num_of_work_units);

enum pilot_export_format_t {
    CSV
};
/**
 * \brief Export all data of a workload to a file
 * @param[in] wl pointer to the workload struct
 * @param format file format to use
 * @param[in] filename the file name to use
 * @return 0 on success; aborts if wl is NULL; otherwise returns an error code. On error, call pilot_strerror to
 * get a pointer to the error message.
 */
int pilot_export(const pilot_workload_t *wl, pilot_export_format_t format, const char *filename);

/**
 * \brief Destroy (free) a workload struct
 * @param[in] wl pointer to the workload struct
 * @return 0 on success; aborts if wl is NULL; otherwise returns an error code. On error, call pilot_strerror to
 * get a pointer to the error message.
 */
int pilot_destroy_workload(pilot_workload_t *wl);

enum pilot_log_level_t
{
    lv_trace,
    lv_debug,
    lv_info,
    lv_warning,
    lv_error,
    lv_fatal
};
/**
 * \brief Set the logging level of the library
 * @param log_level
 */
void pilot_set_log_level(pilot_log_level_t log_level);

/**
 * \brief Set the confidence interval for workload
 * @param[in] wl pointer to the workload struct
 * @param ci the new confidence interval
 * @return the old confidence interval
 */
double pilot_set_confidence_interval(pilot_workload_t *wl, double ci);

double pilot_subsession_mean_p(const double *data, size_t n);
double pilot_subsession_cov_p(const double *data, size_t n, size_t q, double sample_mean);
double pilot_subsession_var_p(const double *data, size_t n, size_t q, double sample_mean);
double pilot_subsession_autocorrelation_coefficient_p(const double *data, size_t n, size_t q, double sample_mean);

/**
 * \brief Basic and statistics information of a workload round
 */
#pragma pack(push, 1)
struct pilot_round_info_t {
    size_t work_amount;
    nanosecond_type round_duration;
    size_t* num_of_unit_readings;
    size_t* warm_up_phase_lens;
};
#pragma pack(pop)

/**
 * \brief Get the basic and statistics information of a workload round
 * \details If info is NULL, this function allocates memory for a
 * pilot_round_info_t, fills information, and returns it. If
 * info is provided, its information will be updated and no new
 * memory will be allocated. The allocated memory must eventually
 * be freed by using pilot_free_round_info().
 * @param[in] wl pointer to the workload struct
 * @param info (optional) if provided, it must point to a pilot_round_info_t
 * that was returned by a previous call to pilot_round_info()
 * @return a pointer to a pilot_round_info_t struct
 */
pilot_round_info_t* pilot_round_info(const pilot_workload_t *wl, size_t round, pilot_round_info_t *info = NULL);

/**
 * \brief Basic and statistics information of a workload
 */
#pragma pack(push, 1)
struct pilot_workload_info_t {
    size_t* total_num_of_unit_readings;
    double* unit_readings_mean;
    double* unit_readings_var;
    double* unit_readings_autocorrelation_coefficient;
    size_t* unit_readings_optimal_subsession_size;
    double* unit_readings_optimal_subsession_var;
    double* unit_readings_optimal_subsession_autocorrelation_coefficient;
    double* unit_readings_optimal_subsession_confidence_interval;
    size_t* unit_readings_required_sample_size;
};
#pragma pack(pop)

/**
 * \brief Get the basic and statistics information of a workload
 * \details If info is NULL, this function allocates memory for a
 * pilot_workload_info_t, fills information, and returns it. If
 * info is provided, its information will be updated and no new
 * memory will be allocated. The allocated memory must eventually
 * be freed by using pilot_free_workload_info().
 * @param[in] wl pointer to the workload struct
 * @param info (optional) if provided, it must point to a pilot_workload_info_t
 * that was returned by a previous call to pilot_workload_info()
 * @return a pointer to a pilot_workload_info_t struct
 */
pilot_workload_info_t* pilot_workload_info(const pilot_workload_t *wl, pilot_workload_info_t *info = NULL);

void pilot_free_workload_info(pilot_workload_info_t *info);

void pilot_free_round_info(pilot_round_info_t *info);

/**
 * Dump the workload summary in Markdown text format
 * @param[in] wl pointer to the workload struct
 * @return a memory buffer of text dump that can be directly output.
 * Use pilot_free_text_dump() to free the buffer after using.
 */
char* pilot_text_workload_summary(const pilot_workload_t *wl);

/**
 * Dump the summary of a round in Markdown text format
 * @param[in] wl pointer to the workload struct
 * @param round_id the round ID starting from 0
 * @return a memory buffer of text dump that can be directly output.
 * Use pilot_free_text_dump() to free the buffer after using.
 */
char* pilot_text_round_summary(const pilot_workload_t *wl, size_t round_id);

void pilot_free_text_dump(char *dump);

/**
 * \brief Calculate the optimal subsession size (q) so that autocorrelation coefficient doesn't exceed the limit
 * @param[in] data the input data
 * @param n size of the input data
 * @param max_autocorrelation_coefficient the maximal limit of the autocorrelation coefficient
 * @return the size of subsession (q); -1 if q can't be found (e.g. q would be larger than n)
 */
int pilot_optimal_subsession_size_p(const double *data, size_t n, double max_autocorrelation_coefficient = 0.1);

/**
 * \brief Calculate the width of the confidence interval given subsession size q and confidence level
 * @param[in] data the input data
 * @param n size of the input data
 * @param q size of each subsession
 * @param confidence_level the probability that the real mean falls within the confidence interval, e.g., .95
 * @return the width of the confidence interval
 */
double pilot_subsession_confidence_interval_p(const double *data, size_t n, size_t q, double confidence_level);

/**
 * \brief Calculate the optimal length of the benchmark session given observed
 *        data, confidence interval, confidence level, and maximal
 *        autocorrelation coefficient. This function uses a pointer as data
 *        source.
 * @param[in] data the input data
 * @param n size of the input data
 * @param confidence_interval_width
 * @param confidence_level
 * @param max_autocorrelation_coefficient
 * @return the recommended sample size; -1 if the max_autocorrelation_coefficient cannot be met
 */
ssize_t pilot_optimal_sample_size_p(const double *data, size_t n, double confidence_interval_width, double confidence_level = 0.95, double max_autocorrelation_coefficient = 0.1);

struct pilot_pi_unit_readings_iter_t;

/**
 * \brief Get a forward iterator for going through the unit readings of a PI
 * with warm-up phase removed
 * @param[in] wl pointer to the workload struct
 * @param piid the ID of the PI
 * @return a iterator
 */
pilot_pi_unit_readings_iter_t*
pilot_pi_unit_readings_iter_new(const pilot_workload_t *wl, int piid);

/**
 * \brief Get the value pointed to by the iterator
 * @param[in] iter a iterator get by using pilot_get_pi_unit_readings_iter()
 * @return the value pointed to by the iterator
 */
double pilot_pi_unit_readings_iter_get_val(const pilot_pi_unit_readings_iter_t* iter);

/**
 * \brief Move the iterator to next value and return it
 * @param[in] iter a iterator get by using pilot_get_pi_unit_readings_iter()
 */
void pilot_pi_unit_readings_iter_next(pilot_pi_unit_readings_iter_t* iter);

/**
 * \brief Check if the iterator points to a valid reading
 * @param[in] iter a iterator get by using pilot_get_pi_unit_readings_iter()
 * @return true on yes; false on end of data
 */
bool pilot_pi_unit_readings_iter_valid(const pilot_pi_unit_readings_iter_t* iter);

/**
 * \brief Destroy and free an iterator
 * @param[in] iter a iterator get by using pilot_get_pi_unit_readings_iter()
 */
void pilot_pi_unit_readings_iter_destroy(pilot_pi_unit_readings_iter_t* iter);

/**
 * \brief Import one round of benchmark results into a workload session
 * @param[in] wl pointer to the workload struct
 * @param round the round ID to store the results with. You can replace the data of
 * an existing round, or add a new round by setting the round to the value of
 * pilot_get_num_of_rounds()
 * @param work_amount the amount of work of that round (you can set it to equal
 * num_of_unit_readings if you have no special use for this value)
 * @param round_duration the duration of the round
 * @param[in] readings the readings of each PI
 * @param num_of_unit_readings the number of unit readings
 * @param[in] unit_readings the unit readings of each PI, can be NULL if there
 * is no unit readings in this round
 */
void pilot_import_benchmark_results(pilot_workload_t *wl, int round,
                                    size_t work_amount,
                                    nanosecond_type round_duration,
                                    const double *readings,
                                    size_t num_of_unit_readings,
                                    const double * const *unit_readings);

/**
 * \brief Get the amount of work load that will be used for next round
 * @param[in] wl pointer to the workload struct
 * @return the work amount
 */
size_t pilot_next_round_work_amount(const pilot_workload_t *wl);

#ifdef __cplusplus
}
#endif

#endif /* LIBPILOT_HEADER_LIBPILOT_H_ */
