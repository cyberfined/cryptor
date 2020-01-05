#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf.h>
#include "huff.h"
#include "aes.h"

extern void* _loader_begin;
extern void* _loader_end;

typedef struct {
    char *loader_begin;
    size_t entry_offset;
    size_t *payload_size_patch_offset;
    size_t *key_seed_pacth_offset;
    size_t *iv_seed_patch_offset;
    size_t loader_size;
} loader_t;

static inline void init_loader(loader_t *l) {
    void *loader_begin = (void*)&_loader_begin;
    l->entry_offset = *(size_t*)loader_begin;
    loader_begin += sizeof(size_t);

    l->payload_size_patch_offset = *(void**)loader_begin;
    loader_begin += sizeof(void*);

    l->key_seed_pacth_offset = *(void**)loader_begin;
    loader_begin += sizeof(void*);

    l->iv_seed_patch_offset = *(void**)loader_begin;
    loader_begin += sizeof(void*);

    l->payload_size_patch_offset = (size_t)l->payload_size_patch_offset + loader_begin;
    l->key_seed_pacth_offset = (size_t)l->key_seed_pacth_offset + loader_begin;
    l->iv_seed_patch_offset = (size_t)l->iv_seed_patch_offset + loader_begin;

    l->loader_begin = loader_begin;
    l->loader_size = (void*)&_loader_end - loader_begin;
}

static inline void* map_file(const char *filename, size_t *mapped_size) {
    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        perror("open");
        return NULL;
    }

    struct stat info;
    if(fstat(fd, &info) < 0) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    
    void *mapped = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }
    close(fd);

    *mapped_size = info.st_size;

    return mapped;
}

static inline int check_elf(void *mapped) {
    Elf64_Ehdr *ehdr = mapped;
    if(memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
        fputs("check_elf: wrong elf e_ident\n", stderr);
        return -1;
    }
    if(ehdr->e_type != ET_DYN && ehdr->e_type != ET_EXEC) {
        fputs("check_elf: elf type must be ET_DYN or ET_EXEC\n", stderr);
        return -1;
    }
    if(ehdr->e_machine != EM_X86_64) {
        fputs("check_elf: elf e_machine isn't EM_X86_64\n", stderr);
        return -1;
    }
    if(ehdr->e_version != EV_CURRENT) {
        fputs("check_elf: wrong elf e_version\n", stderr);
        return -1;
    }

    Elf64_Phdr *phdr = mapped + ehdr->e_phoff;
    for(size_t i = 0; i < ehdr->e_phnum && phdr->p_type != PT_LOAD; i++, phdr++) {
        if(phdr->p_type == PT_INTERP) {
            fputs("check_elf: elf file mustn't contain PT_INTERP header\n", stderr);
            return -1;
        }
    }

    return 0;
}

static inline size_t get_encoding_size(void *mapped) {
    Elf64_Ehdr *ehdr = mapped;
    Elf64_Phdr *phdr = mapped + ehdr->e_phoff;
    size_t size = 0;

    for(size_t i = 0; i < ehdr->e_phnum; i++, phdr++) {
        if(phdr->p_type == PT_LOAD)
            size = phdr->p_offset + phdr->p_filesz;
    }

    return size;
}

typedef struct {
    uint8_t entropy_buf[AES_ENTROPY_BUFSIZE];
    size_t key_seed;
    size_t iv_seed;
    struct AES_ctx ctx;
} AES_state_t;

static inline void init_aes_state(AES_state_t *state) {
    for(int i = 0; i < AES_ENTROPY_BUFSIZE; i++)
        state->entropy_buf[i] = rand() & 0xff;
    state->key_seed = rand();
    state->iv_seed = rand();
    AES_init_ctx_iv(&state->ctx, state->entropy_buf, state->key_seed, state->iv_seed);
}

static inline uint8_t* compress_elf(void *mapped, size_t *comp_size) {
    size_t mapped_size = get_encoding_size(mapped);

    uint8_t *comp_buf = malloc(sizeof(size_t) + 512 + mapped_size);
    if(!comp_buf) {
        perror("malloc");
        return NULL;
    }
    if(huffman_encode(mapped, comp_buf, mapped_size, comp_size) < 0) {
        free(comp_buf);
        return NULL;
    }
    return comp_buf;
}

static inline void patch_loader(loader_t *l, AES_state_t *s, size_t payload_size) {
    *l->payload_size_patch_offset = payload_size;
    *l->key_seed_pacth_offset = s->key_seed;
    *l->iv_seed_patch_offset = s->iv_seed;
}

static inline int write_elf_ehdr(int fd, loader_t *loader) {
    Elf64_Ehdr ehdr;

    memset(ehdr.e_ident, 0, sizeof(ehdr.e_ident));
    memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;

    ehdr.e_type = ET_DYN;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + loader->entry_offset;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_shoff = 0;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 1;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;

    if(write(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        perror("write");
        return -1;
    }

    return 0;
}

static inline int write_elf_phdr(int fd, loader_t *loader, size_t payload_size) {
    Elf64_Phdr phdr;
    phdr.p_type = PT_LOAD;
    phdr.p_offset = 0;
    phdr.p_vaddr = 0;
    phdr.p_paddr = 0;
    phdr.p_filesz = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + loader->loader_size + payload_size + AES_ENTROPY_BUFSIZE;
    phdr.p_memsz = phdr.p_filesz;
    phdr.p_flags = PF_R | PF_W | PF_X;
    phdr.p_align = 0x1000;

    if(write(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
        perror("write");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr,
                "Usage: %s <elf> <output>\n"
                "Linux x86_64 elf cryptor\n",
                argv[0]);
        return 1;
    }

    srand(time(NULL));

    loader_t loader;
    init_loader(&loader);

    size_t mapped_size;
    void *mapped = map_file(argv[1], &mapped_size);
    if(!mapped)
        return 1;

    if(check_elf(mapped) < 0) {
        munmap(mapped, mapped_size);
        return 1;
    }

    size_t comp_size;
    uint8_t *comp_buf = compress_elf(mapped, &comp_size);
    if(!comp_buf) {
        munmap(mapped, mapped_size);
        return 1;
    }
    munmap(mapped, mapped_size);

    AES_state_t aes_st;
    init_aes_state(&aes_st);
    AES_CTR_xcrypt_buffer(&aes_st.ctx, comp_buf, comp_size);
    patch_loader(&loader, &aes_st, comp_size);
    memset(&aes_st.ctx, 0, sizeof(aes_st.ctx));

    int out_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if(out_fd < 0) {
        free(comp_buf);
        return 1;
    }

    if(write_elf_ehdr(out_fd, &loader) < 0 ||
       write_elf_phdr(out_fd, &loader, comp_size) < 0)
    {
        free(comp_buf);
        close(out_fd);
        return 1;
    }

    if(write(out_fd, loader.loader_begin, loader.loader_size) != loader.loader_size ||
       write(out_fd, comp_buf, comp_size) != comp_size ||
       write(out_fd, aes_st.entropy_buf, AES_ENTROPY_BUFSIZE) != AES_ENTROPY_BUFSIZE)
    {
        perror("write");
        free(comp_buf);
        close(out_fd);
        return 1;
    }

    free(comp_buf);
    close(out_fd);
}
