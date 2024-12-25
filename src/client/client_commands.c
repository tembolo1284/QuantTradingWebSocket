#include "client/client_commands.h"
#include "utils/logging.h"
#include "protocol/protocol_validation.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static void generate_order_id(char* order_id, size_t size) {
    time_t now = time(NULL);
    snprintf(order_id, size, "ORD%ld", now);
}

Command parse_command(const char* input) {
    Command cmd = {0};
    cmd.type = CMD_INVALID;

    if (!input) {
        LOG_ERROR("Null input command");
        return cmd;
    }

    char buf[256];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* token = strtok(buf, " \t\n");
    if (!token) {
        LOG_ERROR("Empty command");
        return cmd;
    }

    if (strcasecmp(token, "BUY") == 0) {
        cmd.type = CMD_BUY;
    } else if (strcasecmp(token, "SELL") == 0) {
        cmd.type = CMD_SELL;
    } else if (strcasecmp(token, "CANCEL") == 0) {
        cmd.type = CMD_CANCEL;
    } else if (strcasecmp(token, "VIEW") == 0) {
        cmd.type = CMD_VIEW;
    } else if (strcasecmp(token, "HELP") == 0) {
        cmd.type = CMD_HELP;
        return cmd;
    } else if (strcasecmp(token, "QUIT") == 0) {
        cmd.type = CMD_QUIT;
        return cmd;
    } else {
        LOG_ERROR("Invalid command: %s", token);
        return cmd;
    }

    // Parse symbol
    token = strtok(NULL, " \t\n");
    if (!token) {
        LOG_ERROR("Missing symbol");
        cmd.type = CMD_INVALID;
        return cmd;
    }
    strncpy(cmd.symbol, token, sizeof(cmd.symbol) - 1);
    cmd.symbol[sizeof(cmd.symbol) - 1] = '\0';

    if (cmd.type == CMD_VIEW) {
        return cmd;
    }

    if (cmd.type == CMD_CANCEL) {
        strncpy(cmd.order_id, token, sizeof(cmd.order_id) - 1);
        cmd.order_id[sizeof(cmd.order_id) - 1] = '\0';
        return cmd;
    }

    // Parse price
    token = strtok(NULL, " \t\n");
    if (!token) {
        LOG_ERROR("Missing price");
        cmd.type = CMD_INVALID;
        return cmd;
    }
    cmd.price = atof(token);
    if (!validate_price(cmd.price)) {
        LOG_ERROR("Invalid price: %f", cmd.price);
        cmd.type = CMD_INVALID;
        return cmd;
    }

    // Parse quantity
    token = strtok(NULL, " \t\n");
    if (!token) {
        LOG_ERROR("Missing quantity");
        cmd.type = CMD_INVALID;
        return cmd;
    }
    cmd.quantity = atoi(token);
    if (!validate_quantity(cmd.quantity)) {
        LOG_ERROR("Invalid quantity: %d", cmd.quantity);
        cmd.type = CMD_INVALID;
        return cmd;
    }

    // Generate order ID
    generate_order_id(cmd.order_id, sizeof(cmd.order_id));
    LOG_INFO("Command parsed: type=%d, symbol=%s, price=%.2f, qty=%d, order_id=%s",
             cmd.type, cmd.symbol, cmd.price, cmd.quantity, cmd.order_id);
    
    return cmd;
}

char* format_command_as_json(const Command* cmd, const char* trader_id) {
    if (!cmd || !trader_id) {
        LOG_ERROR("Invalid parameters for JSON formatting");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    switch (cmd->type) {
        case CMD_BUY:
        case CMD_SELL:
            cJSON_AddNumberToObject(root, "type", MSG_PLACE_ORDER);
            cJSON_AddStringToObject(root, "order_id", cmd->order_id);
            cJSON_AddStringToObject(root, "trader_id", trader_id);
            cJSON_AddStringToObject(root, "symbol", cmd->symbol);
            cJSON_AddNumberToObject(root, "price", cmd->price);
            cJSON_AddNumberToObject(root, "quantity", cmd->quantity);
            cJSON_AddBoolToObject(root, "is_buy", cmd->type == CMD_BUY);
            break;

        case CMD_CANCEL:
            cJSON_AddNumberToObject(root, "type", MSG_CANCEL_ORDER);
            cJSON_AddStringToObject(root, "order_id", cmd->order_id);
            cJSON_AddStringToObject(root, "trader_id", trader_id);
            break;

        case CMD_VIEW:
            cJSON_AddNumberToObject(root, "type", MSG_REQUEST_BOOK);
            cJSON_AddStringToObject(root, "symbol", cmd->symbol);
            break;

        default:
            LOG_ERROR("Invalid command type for JSON formatting: %d", cmd->type);
            cJSON_Delete(root);
            return NULL;
    }

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_str) {
        LOG_DEBUG("Formatted command as JSON: %s", json_str);
    } else {
        LOG_ERROR("Failed to format command as JSON");
    }

    return json_str;
}

void print_command_help(void) {
    printf("\nAvailable commands:\n");
    printf("  BUY <symbol> <price> <quantity>\n");
    printf("  SELL <symbol> <price> <quantity>\n");
    printf("  CANCEL <order_id>\n");
    printf("  VIEW <symbol>\n");
    printf("  HELP\n");
    printf("  QUIT\n\n");
}
