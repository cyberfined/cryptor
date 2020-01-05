#pragma once

#include <stddef.h>
#include <stdint.h>

int huffman_encode(uint8_t *in_buf, uint8_t *out_buf, uint32_t in_len, size_t *out_len);
