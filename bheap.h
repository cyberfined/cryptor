#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct bheap_node {
    size_t frequency;
    struct bheap_node *left;
    struct bheap_node *right;
    struct bheap_node *parent;
    uint8_t byte;
} bheap_node;
void bheap_node_free(bheap_node *tree);

typedef struct {
    size_t     heap_size;
    size_t     num_nodes;
    bheap_node **nodes;
} bheap;

bheap* bheap_new(size_t heap_size);
bheap_node* bheap_insert(bheap *heap, bheap_node *node);
bheap_node* bheap_pop(bheap *heap);
void bheap_free(bheap *heap);
