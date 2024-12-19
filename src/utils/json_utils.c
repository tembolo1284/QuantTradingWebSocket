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
            LOG_ERROR("JSON parsing error: %s", error_ptr);
        }
        return false;
    }

    // Extract message type
    cJSON* type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON* action_item = cJSON_GetObjectItemCaseSensitive(root, "action");

    if (!cJSON_IsString(type_item)) {
        LOG_ERROR("Missing or invalid 'type' field");
        cJSON_Delete(root);
        return false;
    }

    const char* type_str = type_item->valuestring;
    const char* action_str = action_item ? action_item->valuestring : NULL;

    LOG_DEBUG("Parsing message: type=%s, action=%s", type_str, action_str ? action_str : "null");

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
                LOG_ERROR("Invalid order: missing or invalid symbol/order");
                cJSON_Delete(root);
                return false;
            }

            parsed_msg->type = JSON_MSG_ORDER_ADD;
            
            // Copy the main symbol
            strncpy(parsed_msg->data.order_add.symbol, 
                    symbol_item->valuestring, 
                    sizeof(parsed_msg->data.order_add.symbol) - 1);
            parsed_msg->data.order_add.symbol[sizeof(parsed_msg->data.order_add.symbol) - 1] = '\0';

            // Parse order details
            cJSON* id_item = cJSON_GetObjectItemCaseSensitive(order_item, "id");
            cJSON* price_item = cJSON_GetObjectItemCaseSensitive(order_item, "price");
            cJSON* quantity_item = cJSON_GetObjectItemCaseSensitive(order_item, "quantity");
            cJSON* is_buy_item = cJSON_GetObjectItemCaseSensitive(order_item, "is_buy");
            cJSON* order_symbol_item = cJSON_GetObjectItemCaseSensitive(order_item, "symbol");

            // Validate order details
            if (!cJSON_IsNumber(id_item) || 
                !cJSON_IsNumber(price_item) || 
                !cJSON_IsNumber(quantity_item) || 
                !cJSON_IsBool(is_buy_item) ||
                !cJSON_IsString(order_symbol_item)) {
                LOG_ERROR("Invalid order details: missing required fields");
                cJSON_Delete(root);
                return false;
            }

            // Populate order details
            Order* order = &parsed_msg->data.order_add.order;
            order->id = (uint64_t)id_item->valuedouble;
            order->price = price_item->valuedouble;
            order->quantity = (uint32_t)quantity_item->valueint;
            order->is_buy = cJSON_IsTrue(is_buy_item);
            
            // Copy order symbol
            strncpy(order->symbol, 
                    order_symbol_item->valuestring,
                    sizeof(order->symbol) - 1);
            order->symbol[sizeof(order->symbol) - 1] = '\0';

            LOG_DEBUG("Successfully parsed order: symbol=%s, price=%.2f, quantity=%u, is_buy=%d",
                     order->symbol, order->price, order->quantity, order->is_buy);
        }
        else if (strcmp(action_str, "cancel") == 0) {
            // Parse order cancel message
            cJSON* order_id_item = cJSON_GetObjectItemCaseSensitive(root, "order_id");

            if (!cJSON_IsNumber(order_id_item)) {
                LOG_ERROR("Invalid cancel: missing or invalid order_id");
                cJSON_Delete(root);
                return false;
            }

            parsed_msg->type = JSON_MSG_ORDER_CANCEL;
            parsed_msg->data.order_cancel.order_id = (uint64_t)order_id_item->valuedouble;
            
            LOG_DEBUG("Successfully parsed cancel order: order_id=%lu", 
                     parsed_msg->data.order_cancel.order_id);
        }
    }
    else if (strcmp(type_str, "book") == 0) {
        if (strcmp(action_str, "query") == 0) {
            parsed_msg->type = JSON_MSG_BOOK_QUERY;
            
            // Parse query type
            cJSON* query_type_item = cJSON_GetObjectItemCaseSensitive(root, "query_type");
            if (cJSON_IsString(query_type_item)) {
                if (strcmp(query_type_item->valuestring, "all") == 0) {
                    parsed_msg->data.book_query.type = BOOK_QUERY_ALL;
                    LOG_DEBUG("Parsed book query: type=all");
                } else {
                    parsed_msg->data.book_query.type = BOOK_QUERY_SYMBOL;
                }
            } else {
                // Default to symbol query for backward compatibility
                parsed_msg->data.book_query.type = BOOK_QUERY_SYMBOL;
            }

            // Parse symbol if present
            cJSON* symbol_item = cJSON_GetObjectItemCaseSensitive(root, "symbol");
            if (cJSON_IsString(symbol_item)) {
                strncpy(parsed_msg->data.book_query.symbol, 
                        symbol_item->valuestring, 
                        sizeof(parsed_msg->data.book_query.symbol) - 1);
                parsed_msg->data.book_query.symbol[sizeof(parsed_msg->data.book_query.symbol) - 1] = '\0';
                LOG_DEBUG("Parsed book query: type=symbol, symbol=%s", 
                         parsed_msg->data.book_query.symbol);
            } else if (parsed_msg->data.book_query.type == BOOK_QUERY_SYMBOL) {
                LOG_ERROR("Symbol query missing symbol field");
                cJSON_Delete(root);
                return false;
            }
        }
    }
    else if (strcmp(type_str, "book_response") == 0) {
        parsed_msg->type = JSON_MSG_BOOK_RESPONSE;
        parsed_msg->data.book_response.symbols_count = 0;
        
        cJSON* symbols_array = cJSON_GetObjectItemCaseSensitive(root, "symbols");
        if (!cJSON_IsArray(symbols_array)) {
            LOG_ERROR("Invalid book response: missing symbols array");
            cJSON_Delete(root);
            return false;
        }

        cJSON* symbol_obj = NULL;
        cJSON_ArrayForEach(symbol_obj, symbols_array) {
            if (parsed_msg->data.book_response.symbols_count >= MAX_SYMBOLS) {
                LOG_WARN("Too many symbols in response, truncating");
                break;
            }

            BookSymbol* curr_symbol = &parsed_msg->data.book_response.symbols[
                parsed_msg->data.book_response.symbols_count];

            cJSON* symbol_name = cJSON_GetObjectItemCaseSensitive(symbol_obj, "symbol");
            if (cJSON_IsString(symbol_name)) {
                strncpy(curr_symbol->symbol, symbol_name->valuestring, 
                        sizeof(curr_symbol->symbol) - 1);
                curr_symbol->symbol[sizeof(curr_symbol->symbol) - 1] = '\0';
            }

            // Parse buy orders
            curr_symbol->buy_orders_count = 0;
            cJSON* buy_orders = cJSON_GetObjectItemCaseSensitive(symbol_obj, "buy_orders");
            if (cJSON_IsArray(buy_orders)) {
                cJSON* order_level = NULL;
                cJSON_ArrayForEach(order_level, buy_orders) {
                    if (curr_symbol->buy_orders_count >= MAX_ORDERS_PER_PRICE) break;

                    cJSON* price = cJSON_GetObjectItemCaseSensitive(order_level, "price");
                    cJSON* orders = cJSON_GetObjectItemCaseSensitive(order_level, "orders");
                    
                    if (cJSON_IsNumber(price) && cJSON_IsArray(orders)) {
                        cJSON* order = NULL;
                        cJSON_ArrayForEach(order, orders) {
                            if (curr_symbol->buy_orders_count >= MAX_ORDERS_PER_PRICE) break;

                            BookOrder* curr_order = &curr_symbol->buy_orders[curr_symbol->buy_orders_count];
                            
                            cJSON* id = cJSON_GetObjectItemCaseSensitive(order, "id");
                            cJSON* quantity = cJSON_GetObjectItemCaseSensitive(order, "quantity");
                            
                            if (cJSON_IsNumber(id) && cJSON_IsNumber(quantity)) {
                                curr_order->id = (uint64_t)id->valuedouble;
                                curr_order->price = price->valuedouble;
                                curr_order->quantity = (uint32_t)quantity->valueint;
                                curr_symbol->buy_orders_count++;
                            }
                        }
                    }
                }
            }

            // Parse sell orders
            curr_symbol->sell_orders_count = 0;
            cJSON* sell_orders = cJSON_GetObjectItemCaseSensitive(symbol_obj, "sell_orders");
            if (cJSON_IsArray(sell_orders)) {
                cJSON* order_level = NULL;
                cJSON_ArrayForEach(order_level, sell_orders) {
                    if (curr_symbol->sell_orders_count >= MAX_ORDERS_PER_PRICE) break;

                    cJSON* price = cJSON_GetObjectItemCaseSensitive(order_level, "price");
                    cJSON* orders = cJSON_GetObjectItemCaseSensitive(order_level, "orders");
                    
                    if (cJSON_IsNumber(price) && cJSON_IsArray(orders)) {
                        cJSON* order = NULL;
                        cJSON_ArrayForEach(order, orders) {
                            if (curr_symbol->sell_orders_count >= MAX_ORDERS_PER_PRICE) break;

                            BookOrder* curr_order = &curr_symbol->sell_orders[curr_symbol->sell_orders_count];
                            
                            cJSON* id = cJSON_GetObjectItemCaseSensitive(order, "id");
                            cJSON* quantity = cJSON_GetObjectItemCaseSensitive(order, "quantity");
                            
                            if (cJSON_IsNumber(id) && cJSON_IsNumber(quantity)) {
                                curr_order->id = (uint64_t)id->valuedouble;
                                curr_order->price = price->valuedouble;
                                curr_order->quantity = (uint32_t)quantity->valueint;
                                curr_symbol->sell_orders_count++;
                            }
                        }
                    }
                }
            }

            parsed_msg->data.book_response.symbols_count++;
        }

        LOG_DEBUG("Parsed book response with %zu symbols", 
                 parsed_msg->data.book_response.symbols_count);
    }
    else {
        LOG_ERROR("Unknown message type: %s", type_str);
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}

char* json_serialize_message(const ParsedMessage* parsed_msg) {
    if (!parsed_msg) return NULL;

    cJSON* root = NULL;
    LOG_DEBUG("Serializing message type: %d", parsed_msg->type);

    switch (parsed_msg->type) {
        case JSON_MSG_ORDER_ADD: {
            LOG_DEBUG("Creating order add message for symbol: %s", parsed_msg->data.order_add.symbol);
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "order");
            cJSON_AddStringToObject(root, "action", "add");
            cJSON_AddStringToObject(root, "symbol", parsed_msg->data.order_add.symbol);

            cJSON* order = cJSON_CreateObject();
            cJSON_AddNumberToObject(order, "id", parsed_msg->data.order_add.order.id);
            cJSON_AddNumberToObject(order, "price", parsed_msg->data.order_add.order.price);
            cJSON_AddNumberToObject(order, "quantity", parsed_msg->data.order_add.order.quantity);
            cJSON_AddBoolToObject(order, "is_buy", parsed_msg->data.order_add.order.is_buy);
            cJSON_AddStringToObject(order, "symbol", parsed_msg->data.order_add.order.symbol);

            cJSON_AddItemToObject(root, "order", order);
            LOG_DEBUG("Order message created");
            break;
        }
        case JSON_MSG_ORDER_CANCEL: {
            LOG_DEBUG("Creating order cancel message for order_id: %lu", 
                     parsed_msg->data.order_cancel.order_id);
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "order");
            cJSON_AddStringToObject(root, "action", "cancel");
            cJSON_AddNumberToObject(root, "order_id", parsed_msg->data.order_cancel.order_id);
            LOG_DEBUG("Cancel message created");
            break;
        }
        case JSON_MSG_BOOK_QUERY: {
            LOG_DEBUG("Creating book query message for symbol: %s", 
                     parsed_msg->data.book_query.symbol);
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "book");
            cJSON_AddStringToObject(root, "action", "query");
            
            // Add query type
            if (parsed_msg->data.book_query.type == BOOK_QUERY_ALL) {
                cJSON_AddStringToObject(root, "query_type", "all");
            } else {
                cJSON_AddStringToObject(root, "query_type", "symbol");
                if (strlen(parsed_msg->data.book_query.symbol) > 0) {
                    cJSON_AddStringToObject(root, "symbol", parsed_msg->data.book_query.symbol);
                }
            }
            LOG_DEBUG("Book query message created");
            break;
        }
        case JSON_MSG_BOOK_RESPONSE: {
            LOG_DEBUG("Creating book response message with %zu symbols", 
                     parsed_msg->data.book_response.symbols_count);
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "book_response");
            
            cJSON* symbols_array = cJSON_CreateArray();
            
            for (size_t i = 0; i < parsed_msg->data.book_response.symbols_count; i++) {
                const BookSymbol* symbol = &parsed_msg->data.book_response.symbols[i];
                cJSON* symbol_obj = cJSON_CreateObject();
                
                cJSON_AddStringToObject(symbol_obj, "symbol", symbol->symbol);
                
                // Add buy orders
                cJSON* buy_orders = cJSON_CreateArray();
                for (size_t j = 0; j < symbol->buy_orders_count; j++) {
                    cJSON* order = cJSON_CreateObject();
                    cJSON_AddNumberToObject(order, "id", symbol->buy_orders[j].id);
                    cJSON_AddNumberToObject(order, "price", symbol->buy_orders[j].price);
                    cJSON_AddNumberToObject(order, "quantity", symbol->buy_orders[j].quantity);
                    cJSON_AddItemToArray(buy_orders, order);
                }
                cJSON_AddItemToObject(symbol_obj, "buy_orders", buy_orders);
                
                // Add sell orders
                cJSON* sell_orders = cJSON_CreateArray();
                for (size_t j = 0; j < symbol->sell_orders_count; j++) {
                    cJSON* order = cJSON_CreateObject();
                    cJSON_AddNumberToObject(order, "id", symbol->sell_orders[j].id);
                    cJSON_AddNumberToObject(order, "price", symbol->sell_orders[j].price);
                    cJSON_AddNumberToObject(order, "quantity", symbol->sell_orders[j].quantity);
                    cJSON_AddItemToArray(sell_orders, order);
                }
                cJSON_AddItemToObject(symbol_obj, "sell_orders", sell_orders);
                
                cJSON_AddItemToArray(symbols_array, symbol_obj);
            }
            
            cJSON_AddItemToObject(root, "symbols", symbols_array);
            LOG_DEBUG("Book response message created");
            break;
        }
        default:
            LOG_ERROR("Unknown message type: %d", parsed_msg->type);
            return NULL;
    }

    if (root) {
        char* json_str = cJSON_Print(root);
        LOG_DEBUG("Serialized JSON: %s", json_str ? json_str : "NULL");
        cJSON_Delete(root);
        return json_str;
    }

    LOG_ERROR("Failed to create JSON message");
    return NULL;
}

void json_free_parsed_message(ParsedMessage* parsed_msg) {
    if (parsed_msg) {
        memset(parsed_msg, 0, sizeof(ParsedMessage));
    }
}
