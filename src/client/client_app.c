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

static void handle_signal(int signum) {
    LOG_INFO("Received signal %d, shutting down...", signum);
    running = false;
    if (cmd_line) {
        command_line_stop(cmd_line);
    }
}

static void handle_command(const Command* cmd, void* user_data) {
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
    int msg_type;
    if (!parse_base_message(message, &msg_type)) {
        LOG_ERROR("Failed to parse message type");
        return;
    }

    switch (msg_type) {
        case MSG_TRADE_EXECUTED: {
            TradeMessage trade;
            if (parse_trade_message(message, &trade)) {
                trade_history_add_trade(trade_history, &trade);
                market_monitor_update_trade(market_monitor, &trade);
            }
            break;
        }
        case MSG_BOOK_SNAPSHOT: {
            BookSnapshot snapshot;
            if (parse_book_snapshot(message, &snapshot)) {
                market_monitor_update_book(market_monitor, &snapshot);
                market_monitor_display(market_monitor);
            }
            break;
        }
        // Handle other message types
    }
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
