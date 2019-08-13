#ifndef TOYOTA_UTILS
#define TOYOTA_UTILS

#include <avsystem/commons/log.h>
#include <avsystem/commons/defs.h>
#include <avsystem/commons/utils.h>
#include "anjay/dm.h"

#include "time.h"

// standard log levels
#define log_trace(Module, ...) avs_log(Module, TRACE, __VA_ARGS__)      // anjay log level trace
#define log_debug(Module, ...) avs_log(Module, DEBUG, __VA_ARGS__)      // anjay log level debug
#define log_info(Module, ...)  avs_log(Module, INFO, __VA_ARGS__)       // anjay log level info
#define log_warn(Module, ...)  avs_log(Module, WARNING, __VA_ARGS__)    // anjay log level warning
#define log_error(Module, ...) avs_log(Module, ERROR, __VA_ARGS__)      // anjay log level error

// different colours for output logs
#define ANSI_COLOR_RED     "\x1b[31m" // red
#define ANSI_COLOR_GREEN   "\x1b[32m" // green
#define ANSI_COLOR_YELLOW  "\x1b[33m" // yellow
#define ANSI_COLOR_BLUE    "\x1b[34m" // blue
#define ANSI_COLOR_MAGENTA "\x1b[35m" // magenta
#define ANSI_COLOR_CYAN    "\x1b[36m" // cyan
#define ANSI_COLOR_RESET   "\x1b[0m"  // reset color code

char *get_current_time(void);         // get current time (return value - string (char * pointer))

#endif // TOYOTA_UTILS
