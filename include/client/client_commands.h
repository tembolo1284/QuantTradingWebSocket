#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

#include "protocol/message_types.h"
#include <stdbool.h>

typedef enum {
    CMD_BUY,
    CMD_SELL,
    CMD_CANCEL,
    CMD_VIEW,
    CMD_HELP,
    CMD_QUIT,
    CMD_INVALID
} CommandType;

typedef struct {
    CommandType type;
    char symbol[16];
    double price;
    int quantity;
    char order_id[32];
} Command;

Command parse_command(const char* input);
char* format_command_as_json(const Command* cmd, const char* trader_id);
void print_command_help(void);

#endif /* CLIENT_COMMANDS_H */
