#include "client/ws_client.h"
#include "client/client_commands.h"
#include "client/command_line.h"
#include "client/order_entry.h"
#include "client/trade_history.h"
#include "client/market_monitor.h"
#include "utils/logging.h"
#include "protocol/json_protocol.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


static volatile bool running = true;
static CommandLine* cmd_line = NULL;
static WSClient* client = NULL;
static OrderEntry* order_entry = NULL;
static TradeHistory* trade_history = NULL;
static MarketMonitor* market_monitor = NULL;
static bool is_view_requested = false;

static void handle_signal(int signum) {
    LOG_INFO("Received signal %d, shutting down...", signum);
    running = false;
    if (cmd_line) {
        command_line_stop(cmd_line);
    }
    if (client) {
        ws_client_disconnect(client);
    }
}

static void handle_command(const Command* cmd, void* user_data) {
    if (cmd->type == CMD_QUIT) {
        LOG_INFO("Initiating client shutdown...");
        running = false;
        
        // Stop components in order
        command_line_stop(cmd_line);
        ws_client_disconnect(client);
        
        // Exit immediately
        exit(0);
        return;
    }

    if (cmd->type == CMD_VIEW) {
        is_view_requested = true;
    }

    WSClient* ws_client = (WSClient*)user_data;
    if (!ws_client || !ws_client_is_connected(ws_client)) {
        LOG_ERROR("Not connected to server");
        return;
    }

    char trader_id[32];
    snprintf(trader_id, sizeof(trader_id), "TRADER%d", getpid());

    char* json = format_command_as_json(cmd, trader_id);
    if (json) {
        ws_client_send(ws_client, json, strlen(json));
        free(json);
    }
}

static void handle_connect(WSClient* ws_client, void* user_data) {
    LOG_INFO("Connected to trading server");
}

static void handle_disconnect(WSClient* ws_client, void* user_data) {
    LOG_INFO("Disconnected from trading server");
}

static void handle_message(WSClient* ws_client, const char* message, size_t len, void* user_data) {
    LOG_INFO("Raw message received (length %zu): %.*s", len, (int)len, message);
    
    int msg_type;
    if (!parse_base_message(message, &msg_type)) {
        LOG_ERROR("Failed to parse message type");
        return;
    }

    cJSON* root = cJSON_Parse(message);
    if (!root) {
        LOG_ERROR("Failed to parse JSON message");
        return;
    }

    switch (msg_type) {
        case MSG_ORDER_ACCEPTED: {
            cJSON* order_id_item = cJSON_GetObjectItem(root, "Trade Details");
            if (!order_id_item || !cJSON_IsObject(order_id_item)) {
                LOG_ERROR("Invalid order accepted message format");
                break;
            }

            cJSON* id = cJSON_GetObjectItem(order_id_item, "Order ID");
            cJSON* symbol_item = cJSON_GetObjectItem(order_id_item, "Symbol");
            cJSON* price_item = cJSON_GetObjectItem(order_id_item, "Price");
            cJSON* quantity_item = cJSON_GetObjectItem(order_id_item, "Quantity");
            cJSON* type_item = cJSON_GetObjectItem(order_id_item, "Type");

            if (!id || !cJSON_IsString(id) ||
                !symbol_item || !cJSON_IsString(symbol_item) ||
                !price_item || !cJSON_IsNumber(price_item) ||
                !quantity_item || !cJSON_IsNumber(quantity_item) ||
                !type_item || !cJSON_IsString(type_item)) {
                LOG_ERROR("Invalid order details");
                break;
            }

            printf("\n=== Order Successfully Placed ===\n");
            printf("  Order ID: %s\n", id->valuestring);
            printf("  Symbol:   %s\n", symbol_item->valuestring);
            printf("  Side:     %s\n", type_item->valuestring);
            printf("  Price:    $%.2f\n", price_item->valuedouble);
            printf("  Quantity: %d\n", quantity_item->valueint);
            printf("===============================\n");
            printf("\ntrading> ");
            fflush(stdout);
            break;
        }

        case MSG_ORDER_REJECTED: {
            const char* order_id = cJSON_GetObjectItem(root, "order_id")->valuestring;
            const char* reason = cJSON_GetObjectItem(root, "reason")->valuestring;
            
            printf("\n=== Order Rejected ===\n");
            printf("  Order ID: %s\n", order_id);
            printf("  Reason:   %s\n", reason);
            printf("===================\n");
            printf("\ntrading> ");
            fflush(stdout);
            break;
        }

        case MSG_TRADE_EXECUTED: {
            TradeMessage trade;
            if (parse_trade_message(message, &trade)) {
                trade_history_add_trade(trade_history, &trade);
                market_monitor_update_trade(market_monitor, &trade);
                
                printf("\n=== Trade Executed ===\n");
                printf("  Symbol:    %s\n", trade.symbol);
                printf("  Price:     $%.2f\n", trade.price);
                printf("  Quantity:  %d\n", trade.quantity);
                printf("  Buy ID:    %s\n", trade.buy_order_id);
                printf("  Sell ID:   %s\n", trade.sell_order_id);
                printf("===================\n");
                printf("\ntrading> ");
                fflush(stdout);
            } else {
                LOG_ERROR("Failed to parse trade execution message");
            }
            break;
        }

        case MSG_BOOK_SNAPSHOT: {
            if (is_view_requested) {
                const char* symbol = cJSON_GetObjectItem(root, "symbol")->valuestring;
                cJSON* bids = cJSON_GetObjectItem(root, "bids");
                cJSON* asks = cJSON_GetObjectItem(root, "asks");
            
                if (!bids || !asks) {
                    LOG_ERROR("Invalid book snapshot format");
                    break;
                }
            
                printf("\n=== Order Book: %s ===\n", symbol);
                printf("----------------------------------------\n");
                printf("      BIDS          |        ASKS       \n");
                printf("  Price    Volume   |   Price    Volume \n");
                printf("----------------------------------------\n");
            
                int num_bids = cJSON_GetArraySize(bids);
                int num_asks = cJSON_GetArraySize(asks);
                int max_rows = (num_bids > num_asks) ? num_bids : num_asks;
            
                for (int i = 0; i < max_rows; i++) {
                    if (i < num_bids) {
                        cJSON* bid = cJSON_GetArrayItem(bids, i);
                        double price = cJSON_GetObjectItem(bid, "price")->valuedouble;
                        int quantity = cJSON_GetObjectItem(bid, "quantity")->valueint;
                        printf("%8.2f  %8d  |", price, quantity);
                    } else {
                        printf("                   |");
                    }
                
                    if (i < num_asks) {
                        cJSON* ask = cJSON_GetArrayItem(asks, i);
                        double price = cJSON_GetObjectItem(ask, "price")->valuedouble;
                        int quantity = cJSON_GetObjectItem(ask, "quantity")->valueint;
                        printf("  %8.2f  %8d", price, quantity);
                    }
                    printf("\n");
                }
            
                printf("----------------------------------------\n");
                if (num_bids == 0 && num_asks == 0) {
                    printf("        (Empty Order Book)             \n");
                }
                printf("\ntrading> ");
                fflush(stdout);
                is_view_requested = false;
            }
            break;
        }

        case MSG_SERVER_STATUS: {
            const char* status = cJSON_GetObjectItem(root, "status")->valuestring;
            printf("\nServer Status: %s\n", status);
            printf("\ntrading> ");
            fflush(stdout);
            break;
        }

        default:
            LOG_DEBUG("Received unhandled message type: %d", msg_type);
            break;
    }

    cJSON_Delete(root);
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize logging
    set_log_level(LOG_INFO);
    
    // Initialize components
    WSClientConfig ws_config = {
        .server_host = "localhost",
        .server_port = 8080,
        .reconnect_interval_ms = 5000,
        .ping_interval_ms = 30000
    };

    OrderEntryConfig order_config = {
        .trader_id = "TRADER1",
        .max_orders = 1000,
        .max_notional = 10000000.0
    };

    TradeHistoryConfig history_config = {
        .max_trades = 1000,
        .record_all_trades = true
    };

    MarketMonitorConfig monitor_config = {
        .max_symbols = 100,
        .update_interval_ms = 1000,
        .display_full_depth = false
    };

    client = ws_client_create(&ws_config);
    if (!client) {
        LOG_ERROR("Failed to create WebSocket client");
        return EXIT_FAILURE;
    }

    cmd_line = command_line_create();
    if (!cmd_line) {
        LOG_ERROR("Failed to create command line");
        ws_client_destroy(client);
        return EXIT_FAILURE;
    }

    order_entry = order_entry_create(&order_config);
    trade_history = trade_history_create(&history_config);
    market_monitor = market_monitor_create(&monitor_config);

    // Set up callbacks
    ws_client_set_connect_callback(client, handle_connect, NULL);
    ws_client_set_disconnect_callback(client, handle_disconnect, NULL);
    ws_client_set_message_callback(client, handle_message, NULL);
    command_line_set_callback(cmd_line, handle_command, client);

    // Start components
    if (ws_client_connect(client) != 0) {
        LOG_ERROR("Failed to connect to server");
        goto cleanup;
    }

    if (command_line_start(cmd_line) != 0) {
        LOG_ERROR("Failed to start command line");
        goto cleanup;
    }

    LOG_INFO("Trading client started");
    
    // Main loop
    while (running) {
        sleep(1);
    }

cleanup:
    market_monitor_destroy(market_monitor);
    trade_history_destroy(trade_history);
    order_entry_destroy(order_entry);
    command_line_destroy(cmd_line);
    ws_client_destroy(client);

    LOG_INFO("Trading client shutdown complete");
    return EXIT_SUCCESS;
}
