#include "trading/protocol/messages.h"
#include "utils/logging.h"
#include <cJSON/cJSON.h>
#include <string.h>

char* trade_notification_serialize(const Trade* trade) {
    if (!trade) {
        LOG_ERROR("Cannot serialize NULL trade");
        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "trade");
    cJSON_AddNumberToObject(root, "trade_id", (double)trade->id);
    cJSON_AddStringToObject(root, "symbol", trade->symbol);
    cJSON_AddNumberToObject(root, "price", trade->price);
    cJSON_AddNumberToObject(root, "quantity", trade->quantity);
    cJSON_AddNumberToObject(root, "buy_order_id", (double)trade->buy_order_id);
    cJSON_AddNumberToObject(root, "sell_order_id", (double)trade->sell_order_id);
    cJSON_AddNumberToObject(root, "timestamp", (double)trade->timestamp);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

char* order_response_serialize(uint64_t order_id, bool success, const char* message) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "order_response");
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddNumberToObject(root, "order_id", (double)order_id);
    if (message) {
        cJSON_AddStringToObject(root, "message", message);
    }

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

char* cancel_response_serialize(CancelResult result, uint64_t order_id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "cancel_response");
    cJSON_AddNumberToObject(root, "order_id", (double)order_id);

    const char* status;
    bool success;

    switch (result) {
        case CANCEL_SUCCESS:
            status = "Order cancelled successfully";
            success = true;
            break;
        case CANCEL_ORDER_NOT_FOUND:
            status = "Order not found";
            success = false;
            break;
        case CANCEL_INVALID_BOOK:
            status = "Invalid order book";
            success = false;
            break;
        case CANCEL_ALREADY_FILLED:
            status = "Order already filled";
            success = false;
            break;
        default:
            status = "Unknown error";
            success = false;
    }

    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "message", status);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

MessageType parse_message(const char* json, void* out_message) {
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse JSON message");
        return MESSAGE_UNKNOWN;
    }

    cJSON* type_obj = cJSON_GetObjectItem(root, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        cJSON_Delete(root);
        return MESSAGE_UNKNOWN;
    }

    MessageType msg_type = MESSAGE_UNKNOWN;

    if (strcmp(type_obj->valuestring, "order") == 0) {
        OrderAddMessage* msg = (OrderAddMessage*)out_message;
        msg->type = MESSAGE_ORDER_ADD;
        
        cJSON* symbol = cJSON_GetObjectItem(root, "symbol");
        cJSON* price = cJSON_GetObjectItem(root, "price");
        cJSON* quantity = cJSON_GetObjectItem(root, "quantity");
        cJSON* is_buy = cJSON_GetObjectItem(root, "is_buy");

        if (symbol && price && quantity && is_buy) {
            strncpy(msg->symbol, symbol->valuestring, sizeof(msg->symbol) - 1);
            msg->symbol[sizeof(msg->symbol) - 1] = '\0';
            msg->price = price->valuedouble;
            msg->quantity = (uint32_t)quantity->valueint;
            msg->is_buy = cJSON_IsTrue(is_buy);
            msg_type = MESSAGE_ORDER_ADD;
        }
    }
    else if (strcmp(type_obj->valuestring, "cancel") == 0) {
        OrderCancelMessage* msg = (OrderCancelMessage*)out_message;
        msg->type = MESSAGE_ORDER_CANCEL;
        
        cJSON* order_id = cJSON_GetObjectItem(root, "order_id");
        if (order_id && cJSON_IsNumber(order_id)) {
            msg->order_id = (uint64_t)order_id->valuedouble;
            msg_type = MESSAGE_ORDER_CANCEL;
        }
    }

    cJSON_Delete(root);
    return msg_type;
}

char* book_query_serialize(const BookQueryConfig* config) {
    if (!config) {
        LOG_ERROR("Invalid book query configuration");
        return NULL;
    }

    // Create root JSON object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "book_response");

    // Create symbols array
    cJSON* symbols = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "symbols", symbols);

    // Convert to string
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}
