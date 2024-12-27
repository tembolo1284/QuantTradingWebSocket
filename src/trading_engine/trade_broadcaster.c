#include "trading_engine/trade_broadcaster.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <cJSON/cJSON.h>

struct TradeBroadcaster {
   WSServer* server;
};

TradeBroadcaster* trade_broadcaster_create(WSServer* server) {
   TradeBroadcaster* broadcaster = calloc(1, sizeof(TradeBroadcaster));
   if (!broadcaster) {
       LOG_ERROR("Failed to allocate trade broadcaster");
       return NULL;
   }
   broadcaster->server = server;
   LOG_INFO("Trade broadcaster created");
   return broadcaster;
}

void trade_broadcaster_destroy(TradeBroadcaster* broadcaster) {
   if (!broadcaster) return;
   free(broadcaster);
   LOG_INFO("Trade broadcaster destroyed");
}

void trade_broadcaster_send_trade(TradeBroadcaster* broadcaster,
                               const char* symbol,
                               const char* buy_order_id,
                               const char* sell_order_id,
                               double price,
                               int quantity,
                               time_t timestamp) {
   if (!broadcaster || !symbol || !buy_order_id || !sell_order_id) {
       LOG_ERROR("Invalid parameters for trade broadcast");
       return;
   }

   char time_str[32];
   strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
            localtime(&timestamp));

   cJSON* trade_msg = cJSON_CreateObject();
   cJSON_AddNumberToObject(trade_msg, "type", 102);  // Trade notification type
   
   cJSON* trade_details = cJSON_CreateObject();
   cJSON_AddStringToObject(trade_details, "symbol", symbol);
   cJSON_AddStringToObject(trade_details, "buy_order", buy_order_id);
   cJSON_AddStringToObject(trade_details, "sell_order", sell_order_id);
   cJSON_AddNumberToObject(trade_details, "price", price);
   cJSON_AddNumberToObject(trade_details, "quantity", quantity);
   cJSON_AddStringToObject(trade_details, "time", time_str);
   
   cJSON_AddItemToObject(trade_msg, "trade", trade_details);

   char* json_str = cJSON_Print(trade_msg);
   ws_server_broadcast(broadcaster->server, json_str, strlen(json_str));
   
   free(json_str);
   cJSON_Delete(trade_msg);
   
   LOG_INFO("Trade broadcast sent: %s %.2f x %d", symbol, price, quantity);
}
