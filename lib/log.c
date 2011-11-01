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

#include "log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#ifndef LOG_LEVEL_DEFAULT
#ifdef DEBUG
#define LOG_LEVEL_DEFAULT LOG_DEBUG
#else
#define LOG_LEVEL_DEFAULT LOG_INFO
#endif
#endif

static unsigned int loglevel = LOG_LEVEL_DEFAULT;

void log_vprintf(unsigned int level, const char *format, va_list args) {
	if (level <= loglevel && level <= LOG_LEVEL_MAX) {
		time_t now = time(NULL);
		FILE *out;
		if (level <= LOG_ERROR) {
			out = stderr;
		} else {
			out = stdout;
		}
#ifdef HAVE_LOCALTIME_R
		struct tm nowtm;
		localtime_r(&now, &nowtm);
		fprintf(out, "[%d-%02d-%02d %02d:%02d:%02d] ", nowtm.tm_year + 1900, nowtm.tm_mon, nowtm.tm_mday, nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);
#else
		fprintf(out, "[%ld] ", now);
#endif
		switch (level) {
		case LOG_FATAL:
			fprintf(out, "!!FATAL!! ");
			break;
		case LOG_ERROR:
			fprintf(out, "**ERROR** ");
			break;
		case LOG_WARN:
			fprintf(out, "--WARNING-- ");
			break;
		case LOG_INFO:
			fprintf(out, "INFO ");
			break;
		case LOG_DEBUG:
		default:
			break;
		}
		vfprintf(out, format, args);
		fprintf(out, "\n");
	}
}

void log_printf(unsigned int level, const char *format, ...) {
	va_list args;
	va_start(args, format);
	log_vprintf(level, format, args);
	va_end(args);
}

void log_set_level(unsigned int level) {
	loglevel = level;
}
