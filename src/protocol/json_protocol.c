#include "protocol/json_protocol.h"
#include "protocol/protocol_validation.h"
#include "utils/logging.h"
#include <string.h>
#include <stdlib.h>

static char last_error[256];

bool parse_base_message(const char* json, int* type) {
    if (!json || !type) {
        LOG_ERROR("Invalid parameters for base message parsing");
        return false;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse JSON: %s", json);
        return false;
    }

    cJSON* type_obj = cJSON_GetObjectItem(root, "type");
    if (!type_obj || !cJSON_IsNumber(type_obj)) {
        LOG_ERROR("Missing or invalid message type");
        cJSON_Delete(root);
        return false;
    }

    *type = type_obj->valueint;
    cJSON_Delete(root);
    LOG_DEBUG("Parsed message type: %d", *type);
    return true;
}

char* serialize_order_message(const OrderMessage* order) {
    if (!order) {
        strncpy(last_error, "Null order message", sizeof(last_error));
        LOG_ERROR("Attempt to serialize null order");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "order_id", order->order_id);
    cJSON_AddStringToObject(root, "trader_id", order->trader_id);
    cJSON_AddStringToObject(root, "symbol", order->symbol);
    cJSON_AddNumberToObject(root, "price", order->price);
    cJSON_AddNumberToObject(root, "quantity", order->quantity);
    cJSON_AddBoolToObject(root, "is_buy", order->is_buy);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_str) {
        LOG_DEBUG("Serialized order message: %s", json_str);
    } else {
        LOG_ERROR("Failed to serialize order message");
    }

    return json_str;
}

char* serialize_trade_message(const TradeMessage* trade) {
    if (!trade) {
        strncpy(last_error, "Null trade message", sizeof(last_error));
        LOG_ERROR("Attempt to serialize null trade");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "symbol", trade->symbol);
    cJSON_AddStringToObject(root, "buy_order_id", trade->buy_order_id);
    cJSON_AddStringToObject(root, "sell_order_id", trade->sell_order_id);
    cJSON_AddNumberToObject(root, "price", trade->price);
    cJSON_AddNumberToObject(root, "quantity", trade->quantity);
    cJSON_AddNumberToObject(root, "timestamp", (double)trade->timestamp);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_str) {
        LOG_DEBUG("Serialized trade message: %s", json_str);
    }

    return json_str;
}

char* serialize_book_snapshot(const BookSnapshot* snapshot) {
    if (!snapshot) {
        LOG_ERROR("Attempt to serialize null book snapshot");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "symbol", snapshot->symbol);
    
    cJSON* bids = cJSON_CreateArray();
    cJSON* asks = cJSON_CreateArray();

    for (int i = 0; i < snapshot->num_bids; i++) {
        cJSON* bid = cJSON_CreateObject();
        cJSON_AddNumberToObject(bid, "price", snapshot->bid_prices[i]);
        cJSON_AddNumberToObject(bid, "quantity", snapshot->bid_quantities[i]);
        cJSON_AddItemToArray(bids, bid);
    }

    for (int i = 0; i < snapshot->num_asks; i++) {
        cJSON* ask = cJSON_CreateObject();
        cJSON_AddNumberToObject(ask, "price", snapshot->ask_prices[i]);
        cJSON_AddNumberToObject(ask, "quantity", snapshot->ask_quantities[i]);
        cJSON_AddItemToArray(asks, ask);
    }

    cJSON_AddItemToObject(root, "bids", bids);
    cJSON_AddItemToObject(root, "asks", asks);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_str) {
        LOG_DEBUG("Serialized book snapshot: %s", json_str);
    }

    return json_str;
}

bool parse_order_message(const char* json, OrderMessage* order) {
    if (!json || !order) {
        LOG_ERROR("Invalid parameters for order parsing");
        return false;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse JSON: %s", json);
        return false;
    }

    cJSON* order_id = cJSON_GetObjectItem(root, "order_id");
    cJSON* trader_id = cJSON_GetObjectItem(root, "trader_id");
    cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
    cJSON* price = cJSON_GetObjectItem(root, "price");
    cJSON* quantity = cJSON_GetObjectItem(root, "quantity");
    cJSON* is_buy = cJSON_GetObjectItem(root, "is_buy");

    if (!order_id || !trader_id || !symbol || !price || !quantity || !is_buy) {
        LOG_ERROR("Missing required fields in order JSON");
        cJSON_Delete(root);
        return false;
    }

    strncpy(order->order_id, order_id->valuestring, sizeof(order->order_id) - 1);
    strncpy(order->trader_id, trader_id->valuestring, sizeof(order->trader_id) - 1);
    strncpy(order->symbol, symbol->valuestring, sizeof(order->symbol) - 1);
    order->price = price->valuedouble;
    order->quantity = quantity->valueint;
    order->is_buy = cJSON_IsTrue(is_buy);

    cJSON_Delete(root);
    LOG_DEBUG("Successfully parsed order message");
    return true;
}

bool parse_trade_message(const char* json, TradeMessage* trade) {
    if (!json || !trade) {
        LOG_ERROR("Invalid parameters for trade parsing");
        return false;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse JSON: %s", json);
        return false;
    }

    cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
    cJSON* buy_order_id = cJSON_GetObjectItem(root, "buy_order_id");
    cJSON* sell_order_id = cJSON_GetObjectItem(root, "sell_order_id");
    cJSON* price = cJSON_GetObjectItem(root, "price");
    cJSON* quantity = cJSON_GetObjectItem(root, "quantity");
    cJSON* timestamp = cJSON_GetObjectItem(root, "timestamp");

    if (!symbol || !buy_order_id || !sell_order_id || !price || !quantity || !timestamp) {
        LOG_ERROR("Missing required fields in trade JSON");
        cJSON_Delete(root);
        return false;
    }

    strncpy(trade->symbol, symbol->valuestring, sizeof(trade->symbol) - 1);
    strncpy(trade->buy_order_id, buy_order_id->valuestring, sizeof(trade->buy_order_id) - 1);
    strncpy(trade->sell_order_id, sell_order_id->valuestring, sizeof(trade->sell_order_id) - 1);
    trade->price = price->valuedouble;
    trade->quantity = quantity->valueint;
    trade->timestamp = (int64_t)timestamp->valuedouble;

    cJSON_Delete(root);
    LOG_DEBUG("Successfully parsed trade message");
    return true;
}

bool parse_book_snapshot(const char* json, BookSnapshot* snapshot) {
    if (!json || !snapshot) {
        LOG_ERROR("Invalid parameters for book snapshot parsing");
        return false;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse JSON: %s", json);
        return false;
    }

    cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
    cJSON* bids = cJSON_GetObjectItem(root, "bids");
    cJSON* asks = cJSON_GetObjectItem(root, "asks");

    if (!symbol || !bids || !asks || !cJSON_IsArray(bids) || !cJSON_IsArray(asks)) {
        LOG_ERROR("Invalid book snapshot JSON structure");
        cJSON_Delete(root);
        return false;
    }

    strncpy(snapshot->symbol, symbol->valuestring, sizeof(snapshot->symbol) - 1);
    
    snapshot->num_bids = cJSON_GetArraySize(bids);
    snapshot->num_asks = cJSON_GetArraySize(asks);

    // Allocate arrays
    snapshot->bid_prices = malloc(snapshot->num_bids * sizeof(double));
    snapshot->bid_quantities = malloc(snapshot->num_bids * sizeof(int));
    snapshot->ask_prices = malloc(snapshot->num_asks * sizeof(double));
    snapshot->ask_quantities = malloc(snapshot->num_asks * sizeof(int));

    if (!snapshot->bid_prices || !snapshot->bid_quantities || 
        !snapshot->ask_prices || !snapshot->ask_quantities) {
        LOG_ERROR("Failed to allocate memory for book snapshot arrays");
        free(snapshot->bid_prices);
        free(snapshot->bid_quantities);
        free(snapshot->ask_prices);
        free(snapshot->ask_quantities);
        cJSON_Delete(root);
        return false;
    }

    // Parse bids
    for (int i = 0; i < snapshot->num_bids; i++) {
        cJSON* bid = cJSON_GetArrayItem(bids, i);
        cJSON* price = cJSON_GetObjectItem(bid, "price");
        cJSON* quantity = cJSON_GetObjectItem(bid, "quantity");
        
        if (price && quantity) {
            snapshot->bid_prices[i] = price->valuedouble;
            snapshot->bid_quantities[i] = quantity->valueint;
        }
    }

    // Parse asks
    for (int i = 0; i < snapshot->num_asks; i++) {
        cJSON* ask = cJSON_GetArrayItem(asks, i);
        cJSON* price = cJSON_GetObjectItem(ask, "price");
        cJSON* quantity = cJSON_GetObjectItem(ask, "quantity");
        
        if (price && quantity) {
            snapshot->ask_prices[i] = price->valuedouble;
            snapshot->ask_quantities[i] = quantity->valueint;
        }
    }

    cJSON_Delete(root);
    LOG_DEBUG("Successfully parsed book snapshot");
    return true;
}

const char* get_last_protocol_error(void) {
    return last_error;
}
