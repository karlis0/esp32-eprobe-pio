#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <ctime>

void setupTime();
void createCurrentTimeOutput(time_t timestamp, char *strftime_buf, size_t buf_len, const char *pattern);

#endif