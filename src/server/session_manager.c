#include "server/session_manager.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SUBSCRIPTIONS 100

typedef struct ClientSession {
    WSClient* client;
    char** subscribed_symbols;
    int subscription_count;
    int64_t last_ping_time;
    struct ClientSession* next;
} ClientSession;

struct SessionManager {
    ClientSession* sessions;
    int session_count;
    int max_sessions;
    int session_timeout_ms;
    pthread_mutex_t lock;
};

SessionManager* session_manager_create(const SessionConfig* config) {
    SessionManager* manager = calloc(1, sizeof(SessionManager));
    if (!manager) return NULL;
    
    manager->max_sessions = config->max_sessions;
    manager->session_timeout_ms = config->session_timeout_ms;
    pthread_mutex_init(&manager->lock, NULL);
    
    return manager;
}

void session_manager_destroy(SessionManager* manager) {
    if (!manager) return;
    
    ClientSession* session = manager->sessions;
    while (session) {
        ClientSession* next = session->next;
        for (int i = 0; i < session->subscription_count; i++) {
            free(session->subscribed_symbols[i]);
        }
        free(session->subscribed_symbols);
        free(session);
        session = next;
    }
    
    pthread_mutex_destroy(&manager->lock);
    free(manager);
}

static ClientSession* find_session(SessionManager* manager, WSClient* client) {
    ClientSession* session = manager->sessions;
    while (session) {
        if (session->client == client) {
            return session;
        }
        session = session->next;
    }
    return NULL;
}

int session_manager_add_client(SessionManager* manager, WSClient* client) {
    if (!manager || !client || manager->session_count >= manager->max_sessions) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    if (find_session(manager, client)) {
        pthread_mutex_unlock(&manager->lock);
        return -1; // Already exists
    }
    
    ClientSession* session = calloc(1, sizeof(ClientSession));
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    session->client = client;
    session->last_ping_time = time(NULL);
    session->subscribed_symbols = calloc(MAX_SUBSCRIPTIONS, sizeof(char*));
    
    if (!session->subscribed_symbols) {
        free(session);
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    session->next = manager->sessions;
    manager->sessions = session;
    manager->session_count++;
    
    pthread_mutex_unlock(&manager->lock);
    return 0;
}

int session_manager_remove_client(SessionManager* manager, WSClient* client) {
    if (!manager || !client) return -1;
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* prev = NULL;
    ClientSession* curr = manager->sessions;
    
    while (curr) {
        if (curr->client == client) {
            if (prev) {
                prev->next = curr->next;
            } else {
                manager->sessions = curr->next;
            }
            
            for (int i = 0; i < curr->subscription_count; i++) {
                free(curr->subscribed_symbols[i]);
            }
            free(curr->subscribed_symbols);
            free(curr);
            manager->session_count--;
            
            pthread_mutex_unlock(&manager->lock);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&manager->lock);
    return -1;
}

int session_manager_subscribe_symbol(SessionManager* manager, WSClient* client, const char* symbol) {
    if (!manager || !client || !symbol) return -1;
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* session = find_session(manager, client);
    if (!session || session->subscription_count >= MAX_SUBSCRIPTIONS) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    // Check if already subscribed
    for (int i = 0; i < session->subscription_count; i++) {
        if (strcmp(session->subscribed_symbols[i], symbol) == 0) {
            pthread_mutex_unlock(&manager->lock);
            return 0;
        }
    }
    
    char* symbol_copy = strdup(symbol);
    if (!symbol_copy) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    session->subscribed_symbols[session->subscription_count++] = symbol_copy;
    
    pthread_mutex_unlock(&manager->lock);
    return 0;
}

int session_manager_unsubscribe_symbol(SessionManager* manager, WSClient* client, const char* symbol) {
    if (!manager || !client || !symbol) return -1;
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* session = find_session(manager, client);
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return -1;
    }
    
    for (int i = 0; i < session->subscription_count; i++) {
        if (strcmp(session->subscribed_symbols[i], symbol) == 0) {
            free(session->subscribed_symbols[i]);
            // Move remaining symbols down
            for (int j = i; j < session->subscription_count - 1; j++) {
                session->subscribed_symbols[j] = session->subscribed_symbols[j + 1];
            }
            session->subscription_count--;
            
            pthread_mutex_unlock(&manager->lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return -1;
}

bool session_manager_is_subscribed(SessionManager* manager, WSClient* client, const char* symbol) {
    if (!manager || !client || !symbol) return false;
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* session = find_session(manager, client);
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return false;
    }
    
    for (int i = 0; i < session->subscription_count; i++) {
        if (strcmp(session->subscribed_symbols[i], symbol) == 0) {
            pthread_mutex_unlock(&manager->lock);
            return true;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return false;
}

int session_manager_get_subscribers(SessionManager* manager, const char* symbol, 
                                  WSClient** clients, int max_clients) {
    if (!manager || !symbol || !clients || max_clients <= 0) return -1;
    
    int count = 0;
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* session = manager->sessions;
    while (session && count < max_clients) {
        for (int i = 0; i < session->subscription_count; i++) {
            if (strcmp(session->subscribed_symbols[i], symbol) == 0) {
                clients[count++] = session->client;
                break;
            }
        }
        session = session->next;
    }
    
    pthread_mutex_unlock(&manager->lock);
    return count;
}

void session_manager_cleanup_sessions(SessionManager* manager) {
    if (!manager) return;
    
    int64_t current_time = time(NULL);
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* prev = NULL;
    ClientSession* curr = manager->sessions;
    
    while (curr) {
        if ((current_time - curr->last_ping_time) * 1000 > manager->session_timeout_ms) {
            ClientSession* to_remove = curr;
            
            if (prev) {
                prev->next = curr->next;
                curr = curr->next;
            } else {
                manager->sessions = curr->next;
                curr = curr->next;
            }
            
            LOG_INFO("Removing timed out session for client");
            for (int i = 0; i < to_remove->subscription_count; i++) {
                free(to_remove->subscribed_symbols[i]);
            }
            free(to_remove->subscribed_symbols);
            free(to_remove);
            manager->session_count--;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
}

void session_manager_ping_clients(SessionManager* manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->lock);
    
    ClientSession* session = manager->sessions;
    while (session) {
        session->last_ping_time = time(NULL);
        session = session->next;
    }
    
    pthread_mutex_unlock(&manager->lock);
}
