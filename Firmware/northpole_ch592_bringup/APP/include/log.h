#ifndef LOG_H
#define LOG_H

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} log_level_t;

void log_init(log_level_t level);
void log_set_level(log_level_t level);
void log_printf(log_level_t level, const char *fmt, ...);
void log_write(const char *text);

#define LOG_ERROR(...) log_printf(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) log_printf(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) log_printf(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_printf(LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif /* LOG_H */
