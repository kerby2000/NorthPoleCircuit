#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

typedef int (*shell_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    shell_handler_t handler;
} shell_command_t;

void shell_init(void);
void shell_register(const shell_command_t *commands, size_t count);
void shell_poll(void);
int shell_execute_line(char *line);

#endif /* SHELL_H */
