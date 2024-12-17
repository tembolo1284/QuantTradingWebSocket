#include "trading/order_book.h"
#include "net/websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#define MAX_PRICE_LEVELS 10
static volatile bool running = true;

typedef struct {
    double price;
    uint32_t quantity;
} PriceLevel;

typedef struct {
    OrderBook* book;
    PriceLevel bids[MAX_PRICE_LEVELS];
    PriceLevel asks[MAX_PRICE_LEVELS];
} OrderBookViewer;

static void handle_signal(int sig) {
    running = false;
}

static void clear_screen(void) {
    printf("\033[2J\033[H");  // ANSI escape codes to clear screen and move cursor to home
}

static void display_order_book(const OrderBookViewer* viewer) {
    clear_screen();
    printf("\033[1;32m%-20s | %-20s\033[0m\n", "BIDS", "ASKS");
    printf("----------------------------------------\n");
    printf("%-10s %-9s | %-10s %-9s\n", "Price", "Quantity", "Price", "Quantity");
    printf("----------------------------------------\n");

    for (int i = 0; i < MAX_PRICE_LEVELS; i++) {
        if (viewer->bids[i].quantity > 0 || viewer->asks[i].quantity > 0) {
            printf("%-.2f %9u | %-.2f %9u\n",
                   viewer->bids[i].price, viewer->bids[i].quantity,
                   viewer->asks[i].price, viewer->asks[i].quantity);
        }
    }
    printf("\nPress 'q' to quit\n");
}

static void on_message(const uint8_t* data, size_t len, void* user_data) {
    OrderBookViewer* viewer = (OrderBookViewer*)user_data;
    
    // In a real system, parse market data message to update order book
    // For this example, we'll just use sample data
    
    // Sample bid levels
    for (int i = 0; i < MAX_PRICE_LEVELS; i++) {
        viewer->bids[i].price = 150.0 - i * 0.1;
        viewer->bids[i].quantity = 100 * (MAX_PRICE_LEVELS - i);
    }
    
    // Sample ask levels
    for (int i = 0; i < MAX_PRICE_LEVELS; i++) {
        viewer->asks[i].price = 150.1 + i * 0.1;
        viewer->asks[i].quantity = 100 * (MAX_PRICE_LEVELS - i);
    }
    
    display_order_book(viewer);
}

static void on_error(ErrorCode error, void* user_data) {
    printf("WebSocket error: %d\n", error);
}

// Set terminal to raw mode to read single keystrokes
static void set_raw_mode(void) {
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &term);
}

// Reset terminal to normal mode
static void reset_term_mode(void) {
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(0, TCSANOW, &term);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    OrderBookViewer viewer = {0};
    viewer.book = order_book_create("AAPL");
    if (!viewer.book) {
        fprintf(stderr, "Failed to create order book\n");
        return 1;
    }

    WebSocketCallbacks callbacks = {
        .on_message = on_message,
        .on_error = on_error,
        .user_data = &viewer
    };

    WebSocket* ws = ws_create(argv[1], (uint16_t)atoi(argv[2]), &callbacks);
    if (!ws) {
        fprintf(stderr, "Failed to create WebSocket connection\n");
        order_book_destroy(viewer.book);
        return 1;
    }

    set_raw_mode();
    printf("Order book viewer started. Press 'q' to exit.\n");

    fd_set fds;
    struct timeval tv;
    
    while (running) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout
        
        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1 && c == 'q') {
                break;
            }
        }
        
        ws_process(ws);
    }

    reset_term_mode();
    ws_close(ws);
    order_book_destroy(viewer.book);
    printf("\nOrder book viewer shutdown complete.\n");
    return 0;
}
