#include "utils/json_utils.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

bool json_parse_message(const char* json_str, ParsedMessage* parsed_msg) {
    if (!json_str || !parsed_msg) return false;

    // Initialize to zero
    memset(parsed_msg, 0, sizeof(ParsedMessage));

    // Parse JSON
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "JSON parsing error: %s\n", error_ptr);
        }
        return false;
    }

    // Extract message type
    cJSON* type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON* action_item = cJSON_GetObjectItemCaseSensitive(root, "action");

    if (!cJSON_IsString(type_item)) {
        cJSON_Delete(root);
        return false;
    }

    const char* type_str = type_item->valuestring;
    const char* action_str = action_item ? action_item->valuestring : NULL;

    // Parse based on message type
    if (strcmp(type_str, "order") == 0) {
        if (strcmp(action_str, "add") == 0) {
            // Parse order add message
            cJSON* symbol_item = cJSON_GetObjectItemCaseSensitive(root, "symbol");
            cJSON* order_item = cJSON_GetObjectItemCaseSensitive(root, "order");

            // Validate symbol and order
            if (!cJSON_IsString(symbol_item) || 
                strlen(symbol_item->valuestring) == 0 || 
                !cJSON_IsObject(order_item)) {
                LOG_ERROR("Invalid order: symbol is required and must be non-empty");
                cJSON_Delete(root);
                return false;
            }

            parsed_msg->type = JSON_MSG_ORDER_ADD;
            strncpy(parsed_msg->data.order_add.symbol, 
                    symbol_item->valuestring, 
                    sizeof(parsed_msg->data.order_add.symbol) - 1);
            parsed_msg->data.order_add.symbol[sizeof(parsed_msg->data.order_add.symbol) - 1] = '\0';

            // Parse order details
            cJSON* id_item = cJSON_GetObjectItemCaseSensitive(order_item, "id");
            cJSON* price_item = cJSON_GetObjectItemCaseSensitive(order_item, "price");
            cJSON* quantity_item = cJSON_GetObjectItemCaseSensitive(order_item, "quantity");
            cJSON* is_buy_item = cJSON_GetObjectItemCaseSensitive(order_item, "is_buy");

            // Validate order details
            if (!cJSON_IsNumber(id_item) || 
                !cJSON_IsNumber(price_item) || 
                !cJSON_IsNumber(quantity_item) || 
                !cJSON_IsBool(is_buy_item)) {
                LOG_ERROR("Invalid order details");
                cJSON_Delete(root);
                return false;
            }

            // Populate order details
            parsed_msg->data.order_add.order.id = (uint64_t)id_item->valuedouble;
            parsed_msg->data.order_add.order.price = price_item->valuedouble;
            parsed_msg->data.order_add.order.quantity = (uint32_t)quantity_item->valuedouble;
            parsed_msg->data.order_add.order.is_buy = is_buy_item->valueint;
            
            // Copy symbol to order
            strncpy(parsed_msg->data.order_add.order.symbol, 
                    parsed_msg->data.order_add.symbol, 
                    sizeof(parsed_msg->data.order_add.order.symbol) - 1);
            parsed_msg->data.order_add.order.symbol[sizeof(parsed_msg->data.order_add.order.symbol) - 1] = '\0';
        }
        else if (strcmp(action_str, "cancel") == 0) {
            // Parse order cancel message
            cJSON* order_id_item = cJSON_GetObjectItemCaseSensitive(root, "order_id");

            if (!cJSON_IsNumber(order_id_item)) {
                cJSON_Delete(root);
                return false;
            }

            parsed_msg->type = JSON_MSG_ORDER_CANCEL;
            parsed_msg->data.order_cancel.order_id = (uint64_t)order_id_item->valuedouble;
        }
    }
    else if (strcmp(type_str, "book") == 0) {
        if (strcmp(action_str, "query") == 0) {
            // Parse book query message
            cJSON* symbol_item = cJSON_GetObjectItemCaseSensitive(root, "symbol");

            if (!cJSON_IsString(symbol_item) || 
                strlen(symbol_item->valuestring) == 0) {
                LOG_ERROR("Invalid book query: symbol is required and must be non-empty");
                cJSON_Delete(root);
                return false;
            }

            parsed_msg->type = JSON_MSG_BOOK_QUERY;
            strncpy(parsed_msg->data.book_query.symbol, 
                    symbol_item->valuestring, 
                    sizeof(parsed_msg->data.book_query.symbol) - 1);
        }
    }

    cJSON_Delete(root);
    return true;
}

char* json_serialize_message(const ParsedMessage* parsed_msg) {
    if (!parsed_msg) return NULL;

    cJSON* root = NULL;

    switch (parsed_msg->type) {
        case JSON_MSG_ORDER_ADD: {
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "order");
            cJSON_AddStringToObject(root, "action", "add");
            cJSON_AddStringToObject(root, "symbol", parsed_msg->data.order_add.symbol);

            cJSON* order = cJSON_CreateObject();
            cJSON_AddNumberToObject(order, "id", parsed_msg->data.order_add.order.id);
            cJSON_AddNumberToObject(order, "price", parsed_msg->data.order_add.order.price);
            cJSON_AddNumberToObject(order, "quantity", parsed_msg->data.order_add.order.quantity);
            cJSON_AddBoolToObject(order, "is_buy", parsed_msg->data.order_add.order.is_buy);

            cJSON_AddItemToObject(root, "order", order);
            break;
        }
        case JSON_MSG_ORDER_CANCEL: {
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "order");
            cJSON_AddStringToObject(root, "action", "cancel");
            cJSON_AddNumberToObject(root, "order_id", parsed_msg->data.order_cancel.order_id);
            break;
        }
        case JSON_MSG_BOOK_QUERY: {
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "book");
            cJSON_AddStringToObject(root, "action", "query");
            cJSON_AddStringToObject(root, "symbol", parsed_msg->data.book_query.symbol);
            break;
        }
        default:
            return NULL;
    }

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    return json_str;
}

char* json_serialize_order_book(const OrderBook* book) {
    if (!book) return NULL;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "book_response");
    cJSON_AddStringToObject(root, "symbol", book->symbol);
    cJSON_AddNumberToObject(root, "best_bid", order_book_get_best_bid(book));
    cJSON_AddNumberToObject(root, "best_ask", order_book_get_best_ask(book));

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    return json_str;
}

void json_free_parsed_message(ParsedMessage* parsed_msg) {
    if (parsed_msg) {
        memset(parsed_msg, 0, sizeof(ParsedMessage));
    }
}

