#include "bheap.h"

#include <stdlib.h>
#include <string.h>

#define BHEAP_MIN_SIZE 8

#define parent(i) (i>>1)
#define left(i)   (i<<1)
#define right(i)  ((i<<1)+1)

static inline size_t next_power_of_two(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bheap* bheap_new(size_t heap_size) {
    if(heap_size < BHEAP_MIN_SIZE)
        heap_size = BHEAP_MIN_SIZE;
    else if(heap_size & (heap_size-1))
        heap_size = next_power_of_two(heap_size);

    bheap *heap = malloc(sizeof(bheap));
    if(!heap)
        return NULL;

    heap->heap_size = heap_size;
    heap->num_nodes = 0;
    heap->nodes = malloc(sizeof(bheap_node*) * heap_size);
    if(!heap->nodes) {
        free(heap);
        return NULL;
    }

    return heap;
}

bheap_node* bheap_insert(bheap *heap, bheap_node *node) {
    if(heap->num_nodes == heap->heap_size-1) {
        size_t new_size = heap->heap_size << 1;
        bheap_node **new_nodes = realloc(heap->nodes, sizeof(bheap_node*) * new_size);
        if(!new_nodes)
            return NULL;
        heap->heap_size = new_size;
        heap->nodes = new_nodes;
    }

    bheap_node *new_node = malloc(sizeof(bheap_node));
    if(!new_node)
        return NULL;
    heap->num_nodes++;
    memcpy(new_node, node, sizeof(bheap_node));
    heap->nodes[heap->num_nodes] = new_node;

    int i = heap->num_nodes;
    while(i > 1) {
        int j = parent(i);
        bheap_node *cur = heap->nodes[i];
        bheap_node *par = heap->nodes[j];
        bheap_node *tmp;

        if(cur->frequency >= par->frequency)
            break;

        tmp = par;
        heap->nodes[j] = cur;
        heap->nodes[i] = tmp;

        i = j;
    }

    return new_node;
}

bheap_node* bheap_pop(bheap *heap) {
    if(heap->num_nodes == 1) {
        heap->num_nodes--;
        return heap->nodes[1];
    } else if(heap->num_nodes < 1) {
        return NULL;
    }

    bheap_node *min = heap->nodes[1];
    bheap_node *last = heap->nodes[heap->num_nodes];
    heap->nodes[1] = last;

    if(heap->heap_size > BHEAP_MIN_SIZE && heap->num_nodes == (heap->heap_size >> 2)) {
        size_t new_size = heap->heap_size >> 1;
        bheap_node **new_nodes = realloc(heap->nodes, sizeof(bheap_node*) * new_size);
        if(new_nodes) {
            heap->nodes = new_nodes;
            heap->heap_size = new_size;
        }
    }

    int i = 1;
    for(;;) {
        int li = left(i);
        int ri = right(i);
        int si;
        bheap_node *cur = heap->nodes[i];
        bheap_node *smallest = cur;
        bheap_node *tmp;

        if(li <= heap->num_nodes) {
            bheap_node *left = heap->nodes[li];
            if(left->frequency < smallest->frequency) {
                si = li;
                smallest = left;
            }
        }

        if(ri <= heap->num_nodes) {
            bheap_node *right = heap->nodes[ri];
            if(right->frequency < smallest->frequency) {
                si = ri;
                smallest = right;
            }
        }

        if(smallest == cur)
            break;

        tmp = smallest;
        heap->nodes[si] = cur;
        heap->nodes[i] = tmp;

        i = si;
    }
    heap->num_nodes--;

    return min;
}

void bheap_free(bheap *heap) {
    for(size_t i = 1; i <= heap->num_nodes; i++)
        free(heap->nodes[i]);
    free(heap->nodes);
    free(heap);
}

void bheap_node_free(bheap_node *tree) {
    while(tree) {
        if(!tree->left && !tree->right) {
            bheap_node *par = tree->parent;
            free(tree);
            if(!par)
                break;
            if(par->left == tree) {
                tree = par->right;
                par->left = NULL;
            } else {
                tree = par;
                tree->right = NULL;
            }
        } else {
            tree = tree->left;
        }
    }
}
