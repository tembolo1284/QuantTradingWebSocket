#ifndef CLIENT_COMMAND_LINE_H
#define CLIENT_COMMAND_LINE_H

#include "client_commands.h"

typedef struct CommandLine CommandLine;
typedef void (*CommandCallback)(const Command* cmd, void* user_data);

CommandLine* command_line_create(void);
void command_line_destroy(CommandLine* cmd_line);

void command_line_set_callback(CommandLine* cmd_line, CommandCallback callback, void* user_data);
int command_line_start(CommandLine* cmd_line);
void command_line_stop(CommandLine* cmd_line);

#endif /* CLIENT_COMMAND_LINE_H */
