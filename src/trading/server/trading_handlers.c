#include "trading/server/trading_handlers.h"
#include "trading/protocol/messages.h"
#include "trading/engine/matcher.h"
#include "utils/logging.h"
#include <string.h>

// Context structure to hold server reference
typedef struct TradingContext {
    WebSocketServer* server;
} TradingContext;

// Callback function for trade notifications
static void trade_notification_callback(const Trade* trade, void* user_data) {
    TradingContext* context = (TradingContext*)user_data;
    if (!context || !context->server) return;

    // Serialize trade notification
    char* notification = trade_notification_serialize(trade);
    if (notification) {
        // Broadcast trade to all clients
        ws_server_broadcast(context->server, 
                          (const uint8_t*)notification, 
                          strlen(notification));
        free(notification);
    }
}

// Handle incoming client messages
void handle_trading_message(WebSocketClient* client, const uint8_t* data, size_t len) {
    if (!client || !data) return;

    // Parse the message
    char* message = strndup((const char*)data, len);
    if (!message) return;

    // Union to hold parsed message data
    union {
        OrderAddMessage order_add;
        OrderCancelMessage order_cancel;
    } msg;

    // Parse the message
    MessageType type = parse_message(message, &msg);
    char* response = NULL;

    switch (type) {
        case MESSAGE_ORDER_ADD: {
            // Create order
            Order* order = order_create(msg.order_add.symbol,
                                     msg.order_add.price,
                                     msg.order_add.quantity,
                                     msg.order_add.is_buy);
            
            if (order) {
                // Add order to book
                OrderHandlingResult result = order_handler_add_order(order);
                
                // Generate response
                response = order_response_serialize(
                    order->id,
                    result == ORDER_SUCCESS,
                    result == ORDER_SUCCESS ? "Order accepted" : "Order rejected"
                );
                
                free(order);
            }
            break;
        }
        
        case MESSAGE_TRADE: {
            // Log trade details
            LOG_INFO("Trade Notification Received:");
            LOG_INFO("  Symbol: %s", msg.trade.symbol);
            LOG_INFO("  Quantity: %u", msg.trade.quantity);
            LOG_INFO("  Price: %.2f", msg.trade.price);
            LOG_INFO("  Buy Order ID: %lu", msg.trade.buy_order_id);
            LOG_INFO("  Sell Order ID: %lu", msg.trade.sell_order_id);
            break;
        }

        case MESSAGE_ORDER_CANCEL: {
            CancelResult result = order_book_cancel(
                order_handler_get_book_by_symbol(NULL),  // TODO: Add symbol to cancel message
                msg.order_cancel.order_id
            );
            
            response = cancel_response_serialize(result, msg.order_cancel.order_id);
            break;
        }

        default:
            LOG_WARN("Unknown message type received");
            break;
    }

    // Send response if generated
    if (response) {
        ws_client_send(client, (const uint8_t*)response, strlen(response));
        free(response);
    }

    free(message);
}

// Initialize trading handlers
TradingContext* trading_handlers_init(WebSocketServer* server) {
    if (!server) return NULL;

    TradingContext* context = malloc(sizeof(TradingContext));
    if (!context) return NULL;

    context->server = server;

    // Set up trade notification callback for all books
    OrderBook** books = malloc(sizeof(OrderBook*) * MAX_SYMBOLS);
    if (books) {
        size_t book_count = order_handler_get_all_books(books, MAX_SYMBOLS);
        for (size_t i = 0; i < book_count; i++) {
            order_book_set_trade_callback(books[i], 
                                        trade_notification_callback, 
                                        context);
        }
        free(books);
    }

    return context;
}

// Cleanup trading handlers
void trading_handlers_cleanup(TradingContext* context) {
    free(context);
}
