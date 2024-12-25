#include "server/ws_server.h"
#include "server/server_handlers.h"
#include "server/session_manager.h"
#include "server/market_data.h"
#include "utils/logging.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static volatile bool running = true;

static void handle_signal(int signum) {
    LOG_INFO("Received signal %d, shutting down...", signum);
    running = false;
}

int main(int argc, char* argv[]) {
    // Initialize logging
    set_log_level(LOG_INFO);
    LOG_INFO("Starting trading server...");

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize components
    WSServerConfig ws_config = {
        .host = "0.0.0.0",
        .port = 8080,
        .max_clients = 100,
        .ping_interval_ms = 30000,
        .status_interval_ms = 60000
    };

    HandlerConfig handler_config = {
        .thread_pool_size = 4,
        .max_message_size = 4096,
        .message_queue_size = 1000
    };

    SessionConfig session_config = {
        .max_sessions = 100,
        .session_timeout_ms = 30000,
        .cleanup_interval_ms = 60000
    };

    MarketDataConfig market_config = {
        .snapshot_interval_ms = 1000,
        .max_depth = 10,
        .max_symbols = 100
    };

    // Create server components
    WSServer* server = ws_server_create(&ws_config);
    if (!server) {
        LOG_ERROR("Failed to create WebSocket server");
        return EXIT_FAILURE;
    }

    ServerHandlers* handlers = server_handlers_create(&handler_config);
    if (!handlers) {
        LOG_ERROR("Failed to create server handlers");
        ws_server_destroy(server);
        return EXIT_FAILURE;
    }

    SessionManager* sessions = session_manager_create(&session_config);
    if (!sessions) {
        LOG_ERROR("Failed to create session manager");
        server_handlers_destroy(handlers);
        ws_server_destroy(server);
        return EXIT_FAILURE;
    }

    MarketData* market = market_data_create(&market_config);
    if (!market) {
        LOG_ERROR("Failed to create market data manager");
        session_manager_destroy(sessions);
        server_handlers_destroy(handlers);
        ws_server_destroy(server);
        return EXIT_FAILURE;
    }

    // Start server
    if (ws_server_start(server) != 0) {
        LOG_ERROR("Failed to start WebSocket server");
        market_data_destroy(market);
        session_manager_destroy(sessions);
        server_handlers_destroy(handlers);
        ws_server_destroy(server);
        return EXIT_FAILURE;
    }

    // Start worker threads
    server_handlers_start_workers(handlers);
    market_data_start_snapshot_timer(market);

    LOG_INFO("Trading server started successfully");

    // Main loop
    while (running) {
        session_manager_cleanup_sessions(sessions);
        session_manager_ping_clients(sessions);
        sleep(1);
    }

    // Cleanup
    LOG_INFO("Shutting down trading server...");
    market_data_stop_snapshot_timer(market);
    server_handlers_stop_workers(handlers);
    ws_server_stop(server);

    market_data_destroy(market);
    session_manager_destroy(sessions);
    server_handlers_destroy(handlers);
    ws_server_destroy(server);

    LOG_INFO("Trading server shutdown complete");
    return EXIT_SUCCESS;
}
