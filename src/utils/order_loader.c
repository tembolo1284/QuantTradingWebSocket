#include "utils/order_loader.h"
#include "utils/logging.h"
#include "trading_engine/order.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define CSV_DELIMITER ","
#define TXT_DELIMITER " \t"  // Space or tab for txt files

typedef enum {
    FILE_TYPE_CSV,
    FILE_TYPE_TXT,
    FILE_TYPE_UNKNOWN
} FileType;

static FileType determine_file_type(const char* filename) {
    const char* extension = strrchr(filename, '.');
    if (!extension) {
        return FILE_TYPE_UNKNOWN;
    }
    
    extension++; // Skip the dot
    if (strcasecmp(extension, "csv") == 0) {
        return FILE_TYPE_CSV;
    } else if (strcasecmp(extension, "txt") == 0) {
        return FILE_TYPE_TXT;
    }
    
    return FILE_TYPE_UNKNOWN;
}

static char* trim_whitespace(char* str) {
    if (!str) return NULL;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;  // All spaces

    // Trim trailing space
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

static Order* parse_order_line(char* line, FileType file_type, int line_number) {
    char order_id[MAX_ID_LENGTH];
    char trader_id[MAX_ID_LENGTH];
    char symbol[MAX_SYMBOL_LENGTH];
    char side[10];
    double price;
    int quantity;
    const char* delimiter = (file_type == FILE_TYPE_CSV) ? CSV_DELIMITER : TXT_DELIMITER;
    
    // Parse line based on file type
    char* token = strtok(line, delimiter);
    if (!token) return NULL;
    strncpy(order_id, trim_whitespace(token), MAX_ID_LENGTH - 1);
    order_id[MAX_ID_LENGTH - 1] = '\0';

    token = strtok(NULL, delimiter);
    if (!token) {
        LOG_ERROR("Line %d: Missing trader ID", line_number);
        return NULL;
    }
    strncpy(trader_id, trim_whitespace(token), MAX_ID_LENGTH - 1);
    trader_id[MAX_ID_LENGTH - 1] = '\0';

    token = strtok(NULL, delimiter);
    if (!token) {
        LOG_ERROR("Line %d: Missing symbol", line_number);
        return NULL;
    }
    strncpy(symbol, trim_whitespace(token), MAX_SYMBOL_LENGTH - 1);
    symbol[MAX_SYMBOL_LENGTH - 1] = '\0';

    token = strtok(NULL, delimiter);
    if (!token) {
        LOG_ERROR("Line %d: Missing side", line_number);
        return NULL;
    }
    strncpy(side, trim_whitespace(token), sizeof(side) - 1);
    side[sizeof(side) - 1] = '\0';

    token = strtok(NULL, delimiter);
    if (!token) {
        LOG_ERROR("Line %d: Missing price", line_number);
        return NULL;
    }
    price = atof(trim_whitespace(token));
    if (price <= 0.0) {
        LOG_ERROR("Line %d: Invalid price %.2f", line_number, price);
        return NULL;
    }

    token = strtok(NULL, delimiter);
    if (!token) {
        LOG_ERROR("Line %d: Missing quantity", line_number);
        return NULL;
    }
    quantity = atoi(trim_whitespace(token));
    if (quantity <= 0) {
        LOG_ERROR("Line %d: Invalid quantity %d", line_number, quantity);
        return NULL;
    }

    // Create order
    bool is_buy = (strcasecmp(side, "BUY") == 0);
    return order_create(order_id, trader_id, symbol, price, quantity, is_buy);
}

int load_orders_from_file(const char* filename, OrderBook* book) {
    if (!filename || !book) {
        LOG_ERROR("Invalid parameters for loading orders");
        return -1;
    }

    FileType file_type = determine_file_type(filename);
    if (file_type == FILE_TYPE_UNKNOWN) {
        LOG_ERROR("Unsupported file type for %s", filename);
        return -1;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Failed to open file %s: %s", filename, strerror(errno));
        return -1;
    }

    LOG_INFO("Loading orders from %s file: %s", 
             (file_type == FILE_TYPE_CSV) ? "CSV" : "TXT", 
             filename);

    char line[MAX_LINE_LENGTH];
    int orders_loaded = 0;
    int line_number = 0;

    // Skip header for CSV files only
    if (file_type == FILE_TYPE_CSV) {
        if (fgets(line, sizeof(line), file) == NULL) {
            LOG_ERROR("File %s is empty", filename);
            fclose(file);
            return -1;
        }
        line_number++;
    }

    // Process each line
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        // Skip empty lines and comments
        char* trimmed_line = trim_whitespace(line);
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '#') {
            continue;
        }

        // Parse and create order
        Order* order = parse_order_line(line, file_type, line_number);
        if (!order) {
            continue;
        }

        // Add order to book
        if (order_book_add_order(book, order) == 0) {
            orders_loaded++;
            LOG_INFO("Loaded order: %s", order_to_string(order));
        } else {
            LOG_ERROR("Line %d: Failed to add order to book", line_number);
            order_destroy(order);
        }
    }

    LOG_INFO("Successfully loaded %d orders from %s", orders_loaded, filename);
    fclose(file);
    return orders_loaded;
}
