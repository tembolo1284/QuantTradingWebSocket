#include "client/command_line.h"
#include "utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_CMD_LEN 256

struct CommandLine {
    pthread_t input_thread;
    bool running;
    CommandCallback callback;
    void* user_data;
};

static void* input_thread(void* arg) {
    CommandLine* cmd_line = (CommandLine*)arg;
    char* line;

    while (cmd_line->running) {
        line = readline("trading> ");
        if (!line) continue;

        if (*line) {
            add_history(line);
            Command cmd = parse_command(line);
            
            if (cmd.type == CMD_QUIT) {
                cmd_line->running = false;
            } else if (cmd.type == CMD_HELP) {
                print_command_help();
            } else if (cmd.type != CMD_INVALID && cmd_line->callback) {
                cmd_line->callback(&cmd, cmd_line->user_data);
            }
        }
        free(line);
    }
    return NULL;
}

CommandLine* command_line_create(void) {
    CommandLine* cmd_line = calloc(1, sizeof(CommandLine));
    if (!cmd_line) {
        LOG_ERROR("Failed to allocate command line");
        return NULL;
    }
    
    using_history();
    LOG_INFO("Command line interface created");
    return cmd_line;
}

void command_line_destroy(CommandLine* cmd_line) {
    if (!cmd_line) return;

    if (cmd_line->running) {
        command_line_stop(cmd_line);
    }

    clear_history();
    free(cmd_line);
    LOG_INFO("Command line interface destroyed");
}

void command_line_set_callback(CommandLine* cmd_line, CommandCallback callback, void* user_data) {
    if (!cmd_line) return;

    cmd_line->callback = callback;
    cmd_line->user_data = user_data;
    LOG_DEBUG("Command callback set");
}

int command_line_start(CommandLine* cmd_line) {
    if (!cmd_line || cmd_line->running) return -1;

    cmd_line->running = true;
    if (pthread_create(&cmd_line->input_thread, NULL, input_thread, cmd_line) != 0) {
        LOG_ERROR("Failed to create input thread");
        cmd_line->running = false;
        return -1;
    }

    LOG_INFO("Command line interface started");
    return 0;
}

void command_line_stop(CommandLine* cmd_line) {
    if (!cmd_line || !cmd_line->running) return;

    cmd_line->running = false;
    pthread_join(cmd_line->input_thread, NULL);
    LOG_INFO("Command line interface stopped");
}
