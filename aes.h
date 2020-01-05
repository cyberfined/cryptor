#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>

#define AES_BLOCKLEN 16
#define AES_KEYLEN 32
#define AES_keyExpSize 240
#define AES_ENTROPY_BUFSIZE 256

struct AES_ctx {
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
};

void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* entropy_buf, uint32_t key_seed, uint32_t iv_seed);
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, uint32_t length);

#endif //_AES_H_
