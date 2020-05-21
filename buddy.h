// Simple buddy allocator.
//
// A buddy allocator allocates based on halved partitions.
// The allocator will halve the partitions as much as possible,
// then give the final halved partition to the allocatee.
//
// Moreover, allocations within free'd partitions can allow for
// coalescing of partitions (i.e. merging partitions).
//
// This leads to minimal external fragmentation, but results in
// internal fragmentation.
//
// In order to use this implementation; in a single translation unit,
// define BUDDY_ALLOC_IMPLEMENTATION then include "buddy.h".
// In all other files, include "buddy.h" normally.
//
// Note: This doesn't actually allocate any memory. The buffer simply
// has a specified size and allocations consist of offset + size.
// This is to be integrated with actual memory (e.g. a GPU buffer).
//
// Example Usage;
// ```
// BuddyBuffer* buf = buddy_buf_new(1024);
// 
// BuddyAllocation alloc;
// if (!buddy_buf_alloc(buf, 30, &alloc)) {
//   printf("allocation failed!");
//   return;
// }
// 
// printf("offset: %zu, size: %zu", alloc.offset, alloc.size);
// 
// buddy_buf_free(alloc); // this is optional
// buddy_buf_destroy(buf);
// ```
//
// This is licensed under the MIT License.

typedef struct BuddyBuffer BuddyBuffer;
typedef struct BuddyNode BuddyNode;

typedef struct BuddyAllocation {
    size_t offset;
    size_t size;
    BuddyNode* node;
} BuddyAllocation;

BuddyBuffer* buddy_buf_new(size_t size);
void buddy_buf_destroy(BuddyBuffer* buf);
int buddy_buf_alloc(BuddyBuffer* buf, size_t size, BuddyAllocation* alloc);
void buddy_buf_free(BuddyAllocation alloc);

#ifdef BUDDY_ALLOC_IMPLEMENTATION

typedef enum BuddyNodeType {
    NODE_TYPE_FREE,
    NODE_TYPE_SPLIT,
    NODE_TYPE_OCCUPIED
} BuddyNodeType;

typedef struct BuddyNode {
    BuddyNodeType type;
    size_t offset;
    size_t size;
    struct BuddyNode* left;
    struct BuddyNode* right;
    struct BuddyNode* parent;
} BuddyNode;

typedef struct BuddyBuffer {
    BuddyNode root;
} BuddyBuffer;

BuddyBuffer* buddy_buf_new(size_t size) {
    BuddyNode root;
    root.type = NODE_TYPE_FREE;
    root.offset = 0;
    root.size = size;
    root.left = NULL;
    root.right = NULL;
    root.parent = NULL;

    BuddyBuffer* buf = (BuddyBuffer*)malloc(sizeof(BuddyBuffer));
    buf->root = root;

    return buf;
}

void destroy_buf_node(BuddyNode* node) {
    if (node->left != NULL) {
        destroy_buf_node(node->left);
    }

    if (node->right != NULL) {
        destroy_buf_node(node->right);
    }

    if (node->parent != NULL) {
        if (node->parent->left == node) {
            node->parent->left = NULL;
        } else {
            node->parent->right = NULL;
        }
        free(node);
    }
}

void buddy_buf_destroy(BuddyBuffer* buf) {
    destroy_buf_node(&buf->root);
    free(buf);
}

BuddyNode* buf_node_new(BuddyNode* parent, int left) {
    BuddyNode* node = (BuddyNode*)malloc(sizeof(BuddyNode));
    node->type = NODE_TYPE_FREE;
    node->offset = parent->offset + (left ? 0 : (parent->size / 2));
    node->size = parent->size / 2;
    node->left = NULL;
    node->right = NULL;
    node->parent = parent;
    return node;
}

int alloc_buf_node(BuddyNode* node, size_t size, BuddyAllocation* alloc) {
    switch (node->type) {
        case NODE_TYPE_FREE: {
            if (node->size / 2 >= size) {
                node->type = NODE_TYPE_SPLIT;
                
                BuddyNode* new_node = buf_node_new(node, 1);
                node->left = new_node;

                return alloc_buf_node(new_node, size, alloc);
            } else if (node->size >= size) {
                node->type = NODE_TYPE_OCCUPIED;

                BuddyAllocation allocation;
                allocation.offset = node->offset;
                allocation.size = node->size;
                allocation.node = node;

                *alloc = allocation;
                return 1;
            } else {
                return 0;
            }
        }
        case NODE_TYPE_SPLIT: {
            if (node->size / 2 < size) {
                return 0;
            }

            if (node->left != NULL) {
                if (alloc_buf_node(node->left, size, alloc)) {
                    return 1;
                }
            } else {
                BuddyNode* new_node = buf_node_new(node, 1);
                node->left = new_node;
                return alloc_buf_node(new_node, size, alloc);
            }

            if (node->right != NULL) {
                if (alloc_buf_node(node->right, size, alloc)) {
                    return 1;
                }
            } else {
                BuddyNode* new_node = buf_node_new(node, 0);
                node->right = new_node;
                return alloc_buf_node(new_node, size, alloc);
            }

            return 0;
        }
        case NODE_TYPE_OCCUPIED: return 0;
    }
}

int buddy_buf_alloc(BuddyBuffer* buf, size_t size, BuddyAllocation* alloc) {
    return alloc_buf_node(&buf->root, size, alloc);
}

void node_update(BuddyNode* node) {
    switch (node->type) {
        case NODE_TYPE_FREE: {
            if (node->parent) {
                BuddyNode* parent = node->parent;
                if (parent->left == node) {
                    destroy_buf_node(parent->left);
                } else {
                    destroy_buf_node(parent->right);
                }
                node_update(parent);
            }
            break;
        }
        case NODE_TYPE_SPLIT: {
            const int can_coalesce =
                (node->left == NULL ||
                    node->left->type == NODE_TYPE_FREE) &&
                (node->right == NULL ||
                    node->right->type == NODE_TYPE_FREE);
            if (can_coalesce) {
                BuddyNode* parent = node->parent;
                destroy_buf_node(node);
                node_update(parent);
            }
            break;
        }
        case NODE_TYPE_OCCUPIED: break;
    }
}

void buddy_buf_free(BuddyAllocation alloc) {
    alloc.node->type = NODE_TYPE_FREE;
    node_update(alloc.node);
}

#endif
