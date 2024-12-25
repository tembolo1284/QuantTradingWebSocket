#ifndef PROTOCOL_CONSTANTS_H
#define PROTOCOL_CONSTANTS_H

// Message format versions
#define PROTOCOL_VERSION 1

// Field size limits
#define MAX_SYMBOL_LENGTH 16
#define MAX_ORDER_ID_LENGTH 32
#define MAX_TRADER_ID_LENGTH 32
#define MAX_MESSAGE_SIZE 4096
#define MAX_BOOK_DEPTH 100

// Validation limits
#define MIN_PRICE 0.0001
#define MAX_PRICE 1000000.0
#define MIN_QUANTITY 1
#define MAX_QUANTITY 1000000

// Error codes
#define ERR_INVALID_MESSAGE -1
#define ERR_INVALID_TYPE -2
#define ERR_INVALID_FIELD -3
#define ERR_PROTOCOL_VERSION -4
#define ERR_MESSAGE_SIZE -5

#endif /* PROTOCOL_CONSTANTS_H */
