#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static log_level_t active_level = LOG_LEVEL_INFO;

FW_WEAK void log_platform_write(const char *text)
{
    fputs(text, stdout);
}

void log_init(log_level_t level)
{
    active_level = level;
}

void log_set_level(log_level_t level)
{
    active_level = level;
}

void log_write(const char *text)
{
    log_platform_write(text);
}

void log_printf(log_level_t level, const char *fmt, ...)
{
    char buffer[192];
    va_list args;

    if (level > active_level) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    log_platform_write(buffer);
}
