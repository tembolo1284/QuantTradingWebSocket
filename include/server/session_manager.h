#ifndef SERVER_SESSION_MANAGER_H
#define SERVER_SESSION_MANAGER_H

#include "ws_server.h"
#include <pthread.h>

typedef struct SessionManager SessionManager;

typedef struct {
    int max_sessions;
    int session_timeout_ms;
    int cleanup_interval_ms;
} SessionConfig;

SessionManager* session_manager_create(const SessionConfig* config);
void session_manager_destroy(SessionManager* manager);

int session_manager_add_client(SessionManager* manager, WSClient* client);
int session_manager_remove_client(SessionManager* manager, WSClient* client);
int session_manager_subscribe_symbol(SessionManager* manager, WSClient* client, const char* symbol);
int session_manager_unsubscribe_symbol(SessionManager* manager, WSClient* client, const char* symbol);

// Symbol subscription queries
bool session_manager_is_subscribed(SessionManager* manager, WSClient* client, const char* symbol);
int session_manager_get_subscribers(SessionManager* manager, const char* symbol, WSClient** clients, int max_clients);

// Session maintenance
void session_manager_ping_clients(SessionManager* manager);
void session_manager_cleanup_sessions(SessionManager* manager);

#endif /* SERVER_SESSION_MANAGER_H */
