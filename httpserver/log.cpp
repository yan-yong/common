#include "log.h"
#include <time.h>
#include <stdarg.h>

 int g_min_log_level = LOG_LEVEL_DEBUG;
bool g_log_need_lock = false;

extern "C" void log_printf(unsigned int flags, FILE* stream, const char* format, ...)
{
    int level = (flags & LOG_LEVEL_MASK);
    if(level < g_min_log_level)
        return;

    char buf[4096];
    time_t t = time(NULL);
    struct tm tm; 
    strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S] ", localtime_r(&t, &tm));

    flockfile(stream);
    fprintf(stream, buf);
    va_list va; 
    va_start(va, format);
    vfprintf(stream, format, va);
    va_end(va);
    funlockfile(stream);

    //va_list va; 
    //va_start(va, format);
    //len += vsnprintf(buf + len, sizeof(buf) - len, format, va);
    //va_end(va);
    //if(g_log_need_lock){
    //    flockfile(stream);
    //    fwrite(buf, 1, len, stream);
    //    funlockfile(stream);
    //}else{
    //    fwrite(buf, 1, len, stream);
    //}   
}

void set_min_log_level(int level)
{
    g_min_log_level = level;
}   

void set_log_need_lock(bool needlock)
{   
    g_log_need_lock = needlock;
}
