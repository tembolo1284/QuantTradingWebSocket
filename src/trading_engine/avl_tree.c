#include "trading_engine/avl_tree.h"
#include "utils/logging.h"
#include "trading_engine/order.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations of static functions
static AVLNode* find_min_node(AVLNode* node);
static AVLNode* find_max_node(AVLNode* node);
static AVLNode* create_node(double price, int64_t timestamp, struct Order* order);
static AVLNode* right_rotate(AVLNode* y);
static AVLNode* left_rotate(AVLNode* x);
static void destroy_node(AVLNode* node);

static int max(int a, int b) {
    return (a > b) ? a : b;
}

static int get_height(AVLNode* node) {
    return node ? node->height : 0;
}

static int get_balance(AVLNode* node) {
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

bool avl_is_empty(const AVLTree* tree) {
    return !tree || !tree->root;
}

static AVLNode* find_min_node(AVLNode* node) {
    if (!node) { 
        LOG_INFO("Node is empty can't find min node");
        return NULL;
    }
    while (node->left) {
        node = node->left;
    }
    return node;
}

static AVLNode* find_max_node(AVLNode* node) {
    if (!node) { 
        LOG_INFO("Node is empty can't find max node");
        return NULL;
    }
    while (node->right) {
        node = node->right;
    }
    return node;
}

static AVLNode* create_node(double price, int64_t timestamp, struct Order* order) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    if (!node) {
        LOG_ERROR("Failed to allocate memory for AVL node");
        return NULL;
    }
    
    node->price = price;
    node->timestamp = timestamp;
    node->order = order;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    
    LOG_DEBUG("Created new AVL node: price=%.2f, timestamp=%ld", price, timestamp);
    return node;
}

static AVLNode* right_rotate(AVLNode* y) {
    LOG_DEBUG("Performing right rotation on node with price=%.2f", y->price);
    
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max(get_height(y->left), get_height(y->right)) + 1;
    x->height = max(get_height(x->left), get_height(x->right)) + 1;

    return x;
}

static AVLNode* left_rotate(AVLNode* x) {
    LOG_DEBUG("Performing left rotation on node with price=%.2f", x->price);
    
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max(get_height(x->left), get_height(x->right)) + 1;
    y->height = max(get_height(y->left), get_height(y->right)) + 1;

    return y;
}

int compare_nodes(double price1, int64_t timestamp1, 
                        double price2, int64_t timestamp2, 
                        bool is_buy_tree) {
    if (price1 != price2) {
        if (is_buy_tree) {
            return price1 > price2 ? 1 : -1;  // Higher prices first for buy orders
        } else {
            return price1 < price2 ? -1 : 1;  // Lower prices first for sell orders
        }
    }
    return timestamp1 < timestamp2 ? -1 : (timestamp1 > timestamp2 ? 1 : 0);
}

static AVLNode* insert_node(AVLNode* node, double price, int64_t timestamp, 
                          struct Order* order, bool is_buy_tree) {
    if (!node) {
        return create_node(price, timestamp, order);
    }

    int cmp = compare_nodes(price, timestamp, node->price, node->timestamp, is_buy_tree);
    if (cmp < 0) {
        node->left = insert_node(node->left, price, timestamp, order, is_buy_tree);
    } else if (cmp > 0) {
        node->right = insert_node(node->right, price, timestamp, order, is_buy_tree);
    } else {
        LOG_WARN("Duplicate node attempted to be inserted: price=%.2f, timestamp=%ld", 
                 price, timestamp);
        return node;
    }

    node->height = max(get_height(node->left), get_height(node->right)) + 1;
    int balance = get_balance(node);

    // Left Left Case
    if (balance > 1 && compare_nodes(price, timestamp, 
                                   node->left->price, 
                                   node->left->timestamp, 
                                   is_buy_tree) < 0) {
        LOG_DEBUG("Performing LL rotation for price=%.2f", price);
        return right_rotate(node);
    }

    // Right Right Case
    if (balance < -1 && compare_nodes(price, timestamp, 
                                    node->right->price, 
                                    node->right->timestamp, 
                                    is_buy_tree) > 0) {
        LOG_DEBUG("Performing RR rotation for price=%.2f", price);
        return left_rotate(node);
    }

    // Left Right Case
    if (balance > 1 && compare_nodes(price, timestamp, 
                                   node->left->price, 
                                   node->left->timestamp, 
                                   is_buy_tree) > 0) {
        LOG_DEBUG("Performing LR rotation for price=%.2f", price);
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }

    // Right Left Case
    if (balance < -1 && compare_nodes(price, timestamp, 
                                    node->right->price, 
                                    node->right->timestamp, 
                                    is_buy_tree) < 0) {
        LOG_DEBUG("Performing RL rotation for price=%.2f", price);
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

static AVLNode* delete_node(AVLNode* root, double price, int64_t timestamp, bool is_buy_tree) {
    if (!root) {
        return NULL;
    }

    int cmp = compare_nodes(price, timestamp, root->price, root->timestamp, is_buy_tree);
    
    if (cmp < 0) {
        root->left = delete_node(root->left, price, timestamp, is_buy_tree);
    } else if (cmp > 0) {
        root->right = delete_node(root->right, price, timestamp, is_buy_tree);
    } else {
        // Node to delete found
        LOG_DEBUG("Found node to delete: price=%.2f, timestamp=%ld", price, timestamp);
        
        // Case 1: No child or one child
        if (!root->left || !root->right) {
            AVLNode* temp = root->left ? root->left : root->right;

            // No child case
            if (!temp) {
                temp = root;
                root = NULL;
            } else {
                // One child case
                *root = *temp; // Copy the contents
            }
            
            LOG_DEBUG("Deleting node with price=%.2f, timestamp=%ld", temp->price, temp->timestamp);
            free(temp);
        } else {
            // Case 2: Two children
            AVLNode* temp = find_min_node(root->right);
            
            // Copy the successor's data
            root->price = temp->price;
            root->timestamp = temp->timestamp;
            root->order = temp->order;

            // Delete the successor
            root->right = delete_node(root->right, temp->price, temp->timestamp, is_buy_tree);
        }
    }

    // If tree had only one node
    if (!root) {
        return NULL;
    }

    // Update height
    root->height = max(get_height(root->left), get_height(root->right)) + 1;

    // Check balance and rebalance if needed
    int balance = get_balance(root);

    // Left Left Case
    if (balance > 1 && get_balance(root->left) >= 0) {
        LOG_DEBUG("Rebalancing: LL case after deletion");
        return right_rotate(root);
    }

    // Left Right Case
    if (balance > 1 && get_balance(root->left) < 0) {
        LOG_DEBUG("Rebalancing: LR case after deletion");
        root->left = left_rotate(root->left);
        return right_rotate(root);
    }

    // Right Right Case
    if (balance < -1 && get_balance(root->right) <= 0) {
        LOG_DEBUG("Rebalancing: RR case after deletion");
        return left_rotate(root);
    }

    // Right Left Case
    if (balance < -1 && get_balance(root->right) > 0) {
        LOG_DEBUG("Rebalancing: RL case after deletion");
        root->right = right_rotate(root->right);
        return left_rotate(root);
    }

    return root;
}

static void destroy_node(AVLNode* node) {
    if (node) {
        destroy_node(node->left);
        destroy_node(node->right);
        LOG_DEBUG("Destroying AVL node: price=%.2f, timestamp=%ld", 
                 node->price, node->timestamp);
        free(node);
    }
}

// Public functions
AVLTree* avl_create(bool is_buy_tree) {
    AVLTree* tree = (AVLTree*)malloc(sizeof(AVLTree));
    if (!tree) {
        LOG_ERROR("Failed to allocate memory for AVL tree");
        return NULL;
    }
    
    tree->root = NULL;
    tree->is_buy_tree = is_buy_tree;
    
    LOG_INFO("Created new AVL tree for %s orders", is_buy_tree ? "buy" : "sell");
    return tree;
}

void avl_destroy(AVLTree* tree) {
    if (tree) {
        LOG_INFO("Destroying AVL tree for %s orders", 
                tree->is_buy_tree ? "buy" : "sell");
        if(tree->root) {
            destroy_node(tree->root);
            tree->root = NULL;
        }
            free(tree);
    }
}

void avl_insert(AVLTree* tree, double price, int64_t timestamp, struct Order* order) {
    if (!tree) {
        LOG_ERROR("Attempted to insert into NULL tree");
        return;
    }
    
    LOG_INFO("Inserting order into %s tree: price=%.2f, timestamp=%ld",
             tree->is_buy_tree ? "buy" : "sell", price, timestamp);
    tree->root = insert_node(tree->root, price, timestamp, order, tree->is_buy_tree);
}

void avl_delete_order(AVLTree* tree, double price, int64_t timestamp) {
    if (!tree) {
        LOG_ERROR("Attempted to delete from NULL tree");
        return;
    }
    
    LOG_INFO("Deleting order from %s tree: price=%.2f, timestamp=%ld",
             tree->is_buy_tree ? "buy" : "sell", price, timestamp);
             
    tree->root = delete_node(tree->root, price, timestamp, tree->is_buy_tree);
}

struct Order* avl_find_min(const AVLTree* tree) {
    if (!tree || !tree->root) {
        LOG_DEBUG("Attempted to find min in empty tree");
        return NULL;
    }
    
    AVLNode* min_node = find_min_node(tree->root);
    if (min_node) {
        LOG_DEBUG("Found min node: price=%.2f, timestamp=%ld",
                 min_node->price, min_node->timestamp);
        return min_node->order;
    }
    return NULL;
}

struct Order* avl_find_max(const AVLTree* tree) {
    if (!tree || !tree->root) {
        LOG_DEBUG("Attempted to find max in empty tree");
        return NULL;
    }
    
    AVLNode* max_node = find_max_node(tree->root);
    if (max_node) {
        LOG_DEBUG("Found max node: price=%.2f, timestamp=%ld",
                 max_node->price, max_node->timestamp);
        return max_node->order;
    }
    return NULL;
}

static void inorder_traverse_helper(AVLNode* node, TraversalCallback callback, void* user_data) {
    if (node) {
        inorder_traverse_helper(node->left, callback, user_data);
        callback(node->order, user_data);
        inorder_traverse_helper(node->right, callback, user_data);
    }
}

void avl_inorder_traverse(const AVLTree* tree, TraversalCallback callback, void* user_data) {
    if (!tree || !callback) {
        LOG_ERROR("Invalid parameters for inorder traversal");
        return;
    }
    
    LOG_DEBUG("Starting inorder traversal of %s tree", 
              tree->is_buy_tree ? "buy" : "sell");
    inorder_traverse_helper(tree->root, callback, user_data);
    LOG_DEBUG("Completed inorder traversal");
}
