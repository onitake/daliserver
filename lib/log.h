/* Copyright (c) 2011, onitake <onitake@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LOG_H
#define _LOG_H

#include "config.h"
#include <stdarg.h>

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

#ifndef LOG_LEVEL_MAX
#ifdef DEBUG
#define LOG_LEVEL_MAX LOG_DEBUG
#else
#define LOG_LEVEL_MAX LOG_INFO
#endif
#endif

// Generic logging functions, they take arguments like printf and vprintf
void log_vprintf(unsigned int level, const char *format, va_list args);
void log_printf(unsigned int level, const char *format, ...);
// Sets the log level
// Setting a level higher than what was compiled in has no effect
void log_set_level(unsigned int level);
// Gets the current log level
unsigned int log_get_level();

// It is recommended to use these convenience functions instead of directly calling log_printf().
// They are optimized out if the maximum debug level set during compilation is lower.
#if LOG_LEVEL_MAX < LOG_FATAL
#define log_fatal(format...)
#else
#define log_fatal(format...) log_printf(LOG_FATAL, format)
#endif
#if LOG_LEVEL_MAX < LOG_ERROR
#define log_error(format...)
#else
#define log_error(format...) log_printf(LOG_ERROR, format)
#endif
#if LOG_LEVEL_MAX < LOG_WARN
#define log_warn(format...)
#else
#define log_warn(format...) log_printf(LOG_WARN, format)
#endif
#if LOG_LEVEL_MAX < LOG_INFO
#define log_info(format...)
#else
#define log_info(format...) log_printf(LOG_INFO, format)
#endif
#if LOG_LEVEL_MAX < LOG_DEBUG
#define log_debug(format...)
#define log_debug_enabled() 0
#else
#define log_debug(format...) log_printf(LOG_DEBUG, format)
#define log_debug_enabled() (log_get_level() >= LOG_DEBUG)
#endif

#endif /*_LOG_H*/
