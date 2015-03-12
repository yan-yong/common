#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __METASEARCH_LOG_H
#define __METASEARCH_LOG_H

#define LOG_LEVEL_NULL    0x00
#define LOG_LEVEL_TRACE   0x01
#define LOG_LEVEL_DEBUG   0x02
#define LOG_LEVEL_INFO    0x03
#define LOG_LEVEL_NOTICE  0x04
#define LOG_LEVEL_WARNING 0x05
#define LOG_LEVEL_ERROR   0x06
#define LOG_LEVEL_MASK    0x07
#define LOG_THREAD        0x40

#define LOG_INFO(format, ...) log_printf(LOG_LEVEL_INFO, stdout, format, ##__VA_ARGS__)
#define LOG_NOTICE(format, ...) log_printf(LOG_LEVEL_NOTICE, stdout, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) log_printf(LOG_LEVEL_WARNING, stdout, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log_printf(LOG_LEVEL_ERROR, stderr, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) log_printf(LOG_LEVEL_DEBUG, stdout, format, ##__VA_ARGS__)

extern "C" void log_printf(unsigned int flags, FILE* stream, const char* format, ...) __attribute__((format(printf, 3, 4)));

#endif
