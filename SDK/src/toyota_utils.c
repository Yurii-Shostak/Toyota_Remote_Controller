#include "toyota_utils.h"

char *get_current_time(void) {
    struct tm *tm_ptr;                    // pointer to time struct
    time_t local_time;                    // local time system struct
    local_time = time(0);                 // get local time in milliseconds
    tm_ptr = localtime(&local_time);      // get local time date and year as string
    return asctime(tm_ptr);
}
