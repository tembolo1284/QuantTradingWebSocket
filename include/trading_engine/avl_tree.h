#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration for Order
struct Order;

typedef struct AVLNode {
    double price;
    int64_t timestamp;
    struct Order* order;
    struct AVLNode* left;
    struct AVLNode* right;
    int height;
} AVLNode;

typedef struct AVLTree {
    AVLNode* root;
    bool is_buy_tree;  // True for buy orders (max heap), False for sell orders (min heap)
} AVLTree;

// Tree operations
AVLTree* avl_create(bool is_buy_tree);
void avl_destroy(AVLTree* tree);
void avl_insert(AVLTree* tree, double price, int64_t timestamp, struct Order* order);
struct Order* avl_find_min(const AVLTree* tree);
struct Order* avl_find_max(const AVLTree* tree);
void avl_delete_order(AVLTree* tree, double price, int64_t timestamp);
bool avl_contains(const AVLTree* tree, double price, int64_t timestamp);
bool avl_is_empty(const AVLTree* tree);

// Helper functions for traversal
typedef void (*TraversalCallback)(struct Order* order, void* user_data);
void avl_inorder_traverse(const AVLTree* tree, TraversalCallback callback, void* user_data);

// Delete operation
void avl_delete_order(AVLTree* tree, double price, int64_t timestamp);

#endif /* AVL_TREE_H */
