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
#include <errno.h>
#include <string.h>
#ifdef HAVE_VSYSLOG
#include <syslog.h>
#endif
#include <stdlib.h>

#ifndef LOG_LEVEL_DEFAULT
#ifdef DEBUG
#define LOG_LEVEL_DEFAULT LOG_LEVEL_DEBUG
#else
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif
#endif

static unsigned int loglevel = LOG_LEVEL_DEFAULT;
static unsigned int loglevel_file = LOG_LEVEL_DEFAULT;
static unsigned int loglevel_syslog = LOG_LEVEL_ERROR;
static char *logfile = NULL;
static FILE *fp_logfile = NULL;
static int enabled_syslog = 0;

void log_vprintf(unsigned int level, const char *format, va_list args) {
	if (level <= LOG_LEVEL_MAX) {
#ifdef HAVE_VSYSLOG
		// Create copies of the argument list to allow multiple vfprintf calls
		va_list syslogargs;
		va_copy(syslogargs, args);
#endif
		if (level <= loglevel || (fp_logfile && level <= loglevel_file)) {
			time_t now = time(NULL);
			FILE *out;
			if (level <= LOG_LEVEL_ERROR) {
				out = stderr;
			} else {
				out = stdout;
			}
			char *datefmt = malloc(32);
#ifdef HAVE_LOCALTIME_R
			struct tm nowtm;
			localtime_r(&now, &nowtm);
			snprintf(datefmt, 32, "[%d-%02d-%02d %02d:%02d:%02d] ", nowtm.tm_year + 1900, nowtm.tm_mon + 1, nowtm.tm_mday, nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);
#else
			snprintf(datefmt, 32, "[%ld] ", now);
#endif
			char *prefixfmt = NULL;
			switch (level) {
			case LOG_LEVEL_FATAL:
				prefixfmt = strdup("!!FATAL!! ");
				break;
			case LOG_LEVEL_ERROR:
				prefixfmt = strdup("**ERROR** ");
				break;
			case LOG_LEVEL_WARN:
				prefixfmt = strdup("--WARNING-- ");
				break;
			case LOG_LEVEL_INFO:
				prefixfmt = strdup("INFO ");
				break;
			case LOG_LEVEL_DEBUG:
			default:
				prefixfmt = strdup("");
				break;
			}
			va_list fileargs;
			if (fp_logfile && level <= loglevel_file) {
				va_copy(fileargs, args);
			}
			if (level <= loglevel) {
				fprintf(out, "%s%s", datefmt, prefixfmt);
				vfprintf(out, format, args);
				fprintf(out, "\n");
				fflush(out);
			}
			if (fp_logfile && level <= loglevel_file) {
				fprintf(fp_logfile, "%s%s", datefmt, prefixfmt);
				vfprintf(fp_logfile, format, fileargs);
				fprintf(fp_logfile, "\n");
				fflush(fp_logfile);
				va_end(fileargs);
			}
			free(prefixfmt);
			free(datefmt);
		}
#ifdef HAVE_VSYSLOG
		if (level <= loglevel_syslog) {
			switch (level) {
			case LOG_LEVEL_FATAL:
				vsyslog(LOG_ALERT, format, syslogargs);
				break;
			case LOG_LEVEL_ERROR:
				vsyslog(LOG_ERR, format, syslogargs);
				break;
			case LOG_LEVEL_WARN:
				vsyslog(LOG_WARNING, format, syslogargs);
				break;
			case LOG_LEVEL_INFO:
				vsyslog(LOG_INFO, format, syslogargs);
				break;
			case LOG_LEVEL_DEBUG:
				vsyslog(LOG_DEBUG, format, syslogargs);
			default:
				break;
			}
		}
		va_end(syslogargs);
#endif
	}
}

void log_printf(unsigned int level, const char *format, ...) {
	va_list args;
	va_start(args, format);
	log_vprintf(level, format, args);
	va_end(args);
}

void log_set_level(unsigned int level) {
	if (level > LOG_LEVEL_MAX) {
		loglevel = LOG_LEVEL_MAX;
	} else {
		loglevel = level;
	}
}

unsigned int log_get_level() {
	return loglevel;
}

int log_reopen_file() {
	if (fp_logfile) {
		fclose(fp_logfile);
		fp_logfile = NULL;
	}
	if (logfile) {
		FILE *fp = fopen(logfile, "a");
		if (!fp) {
			log_error("Error opening log file %s: %s", logfile, strerror(errno));
			return -1;
		} else {
			if (fp_logfile) {
				fclose(fp_logfile);
			}
			fp_logfile = fp;
		}
	}
	return 0;
}

int log_set_logfile(const char *logfile_path) {
	free(logfile);
	logfile = NULL;
	if (logfile_path != NULL) {
		logfile = strdup(logfile_path);
		if (!logfile) {
			log_error("Error setting log file %s: %s", logfile_path, strerror(errno));
			return -1;
		}
	}
	return log_reopen_file();
}

void log_set_logfile_level(unsigned int level) {
	if (level > LOG_LEVEL_MAX) {
		loglevel_file = LOG_LEVEL_MAX;
	} else {
		loglevel_file = level;
	}
}

#ifdef HAVE_VSYSLOG
void log_set_syslog(const char *name) {
	if (enabled_syslog) {
		closelog();
		enabled_syslog = 0;
	}
	if (name) {
		openlog(name, 0, LOG_DAEMON);
		enabled_syslog = 1;
	}
}

void log_set_syslog_level(unsigned int level) {
	if (level > LOG_LEVEL_MAX) {
		loglevel_syslog = LOG_LEVEL_MAX;
	} else {
		loglevel_syslog = level;
	}
}
#endif

