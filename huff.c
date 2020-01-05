#include "huff.h"
#include "bheap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BIT_LIMIT 32

static inline bheap* get_stat(uint8_t *buf, size_t len) {
    size_t freqs[256];
    bheap *heap = NULL;
    bheap_node node;

    memset(freqs, 0, sizeof(freqs));

    for(size_t i = 0; i < len; i++)
        freqs[(uint8_t)buf[i]]++;

    heap = bheap_new(512);
    if(!heap)
        goto error;

    for(int i = 0; i < 256; i++) {
        if(!freqs[i])
            continue;

        node = (bheap_node) {
            .byte = (uint8_t)i,
            .parent = NULL,
            .frequency = freqs[i],
            .left = NULL,
            .right = NULL,
        };

        if(!bheap_insert(heap, &node))
            goto error;
    }

    return heap;
error:
    perror("get_stat");
    if(heap) bheap_free(heap);
    return NULL;
}

static inline bheap_node* build_tree(bheap *heap) {
    bheap_node *f = NULL, *s = NULL;
    while(heap->num_nodes > 1) {
        f = bheap_pop(heap);
        if(!f)
            goto error;

        s = bheap_pop(heap);
        if(!s)
            goto error;

        bheap_node node = (bheap_node) {
            .byte = 0,
            .parent = NULL,
            .frequency = f->frequency + s->frequency,
            .left = f,
            .right = s,
        };

        bheap_node *inserted = bheap_insert(heap, &node);
        if(!inserted)
            goto error;
        f->parent = s->parent = inserted;
        s = NULL;
    }

    return heap->nodes[1];
error:
    perror("build_tree");
    if(f) bheap_node_free(f);
    if(s) bheap_node_free(s);
    return NULL;
}

typedef struct {
    uint32_t code;
    uint8_t  size;
} symbol;

static inline int get_alphabet(bheap_node *tree, symbol *alphabet) {
    uint32_t code = 0;
    uint8_t size = 0;
    bool dir = false;

    while(tree) {
        if(size > BIT_LIMIT) {
            fputs("get_alphabet: bit limit has been violated", stderr);
            return -1;
        }

        if(!tree->left && !tree->right) {
            alphabet[tree->byte] = (symbol) {
                .code = code,
                .size = size,
            };

            if(!tree->parent) {
                break;
            } else if(tree->parent->left == tree) {
                tree = tree->parent->right;
                code |= 1;
                continue;
            } else {
                dir = true;
            }
        } else if(!dir) {
            tree = tree->left;
            code <<= 1;
            size++;
            continue;
        } 

        if(dir) {
            if(!tree->parent) {
                break;
            } else if(tree->parent->right == tree) {
                tree = tree->parent;
                code = (code ^ 1) >> 1;
                size--;
            } else {
                tree = tree->parent->right;
                code |= 1;
                dir = false;
            }
        }
    }

    return 0;
}

static inline size_t write_tree(uint8_t *buf, bheap_node *tree) {
    size_t len = 0;
    uint8_t byte = 0, bits = 8;
    bool dir = false;

    while(tree) {
        if(!tree->left && !tree->right) {
            bits--;
            byte |= (1 << bits);

            uint8_t chr = tree->byte;
            for(int i = 8;;) {
                if(bits == 0) {
                    buf[len++] = byte;
                    byte = 0;
                    bits = 8;
                }

                if(i == 0)
                    break;

                bits--;
                i--;
                byte |= ((chr >> i) & 1) << bits;
            }

            if(!tree->parent) {
                break;
            } else if(tree->parent->left == tree) {
                tree = tree->parent->right;
            } else {
                dir = true;
                tree = tree->parent;
            }
        } else if(!dir) {
            bits--;
            tree = tree->left;
        } else if(!tree->parent) {
            break;
        } else if(tree->parent->right == tree) {
            tree = tree->parent;
        } else {
            tree = tree->parent->right;
            dir = false;
        }

        if(bits == 0) {
            buf[len++] = byte;
            byte = 0;
            bits = 8;
        }
    }

    if(bits != 8)
        buf[len++] = byte;

    return len;
}

static inline size_t encode(uint8_t *in_buf, uint8_t *out_buf, size_t in_len, symbol *alphabet) {
    uint8_t byte = 0;
    size_t out_len=0;
    uint8_t bit_cnt = 8;

    for(size_t i = 0; i < in_len; i++) {
        uint8_t ind = in_buf[i];

        uint8_t sz = alphabet[ind].size;
        uint32_t code = alphabet[ind].code;
        while(sz) {
            if(bit_cnt >= sz) {
                bit_cnt -= sz;
                byte |= (code << bit_cnt);
                sz = 0;
            } else {
                sz -= bit_cnt;
                byte |= (code >> sz);
                bit_cnt = 0;
            }

            if(bit_cnt == 0) {
                out_buf[out_len++] = byte;
                byte = 0;
                bit_cnt = 8;
            }
        }
    }

    if(bit_cnt != 8)
        out_buf[out_len++] = byte;

    return out_len;
}

int huffman_encode(uint8_t *in_buf, uint8_t *out_buf, uint32_t in_len, size_t *out_len) {
    int ret = -1;
    symbol alphabet[256];
    bheap *heap;
    bheap_node *tree = NULL;
    size_t len = 0;

    heap = get_stat(in_buf, in_len);
    if(!heap)
        goto exit;

    tree = build_tree(heap);
    if(!tree) {
        bheap_free(heap);
        goto exit;
    }
    free(heap->nodes);
    free(heap);

    if(get_alphabet(tree, alphabet) < 0)
        goto exit;

    memcpy(out_buf, &in_len, sizeof(in_len));
    len += sizeof(in_len);
    len += write_tree(out_buf+len, tree);
    len += encode(in_buf, out_buf+len, in_len, alphabet);

    *out_len = len;

    ret = 0;
exit:
    if(tree) bheap_node_free(tree);
    return ret;
}
