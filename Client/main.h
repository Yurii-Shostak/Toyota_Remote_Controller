#ifndef MAIN_H
#define MAIN_H

#include <unistd.h>
#include <signal.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <stddef.h>
#include <time.h>
#include <stdbool.h>

#include "../SDK/include/toyota_client.h"
#include "../SDK/include/toyota_utils.h"
#include "../SDK/include/Main_Objects/humidity.h"
#include "../SDK/include/Main_Objects/headlights_control.h"
#include "file_parser.h"

char **saved_argv;
#define DEFAULT_ANJAY_LIFETIME 86400   // default time of registration update
#define DEFAULT_TIME_TO_WAIT   5000000 // default time to wait in microseconds
#define MAX_WAIT_TIME          1000    // max wait time for anjay scheduler
#define MIN(a,b) (((a)<(b))?(a):(b))

#endif // MAIN_H
