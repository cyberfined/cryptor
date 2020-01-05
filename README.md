# cryptor

Compresses and encrypts given x86_64 elf file and creates new elf file, which decrypt, decompress and put in right place in memory given elf file. For compression huffman encoding is used. For encryption aes is used. New elf file has following structure:

| Elf headers                        |
|------------------------------------|
| Cryload                            |
|  Compressed and encrypted elf file |
| 256 byte of random data            |

[cryload](https://github.com/cyberfined/cryload) is a loader for compressed and encrypted elf files. Buffer at the end of file is used to initialize aes key and iv with linear congruential generator.

Only statically linked ET\_EXEC and ET\_DYN elf files are supported. Dynamic linked elf files aren't supported, because there are different versions of shared libraries in different linux distributions, so probability that your program can't find right version of some library is high.

# Bugs

Cryptor isn't working with statically linked with glibc elf files, I don't know why but programs statically linked with glibc causes segfault. Cryptor is tested on statically linked golang programs, statically linked with musl-libc c programs, ET\_DYN c programs without dynamic linker.

# Usage

```bash
./cryptor <input_elf_file> <output_elf_file>
```

# AES implementation

I use [kokke's](https://github.com/kokke/tiny-AES-c) tiny-AES-c implementation.

# load_elf

In cryload I use bediger's [load_elf](https://github.com/bediger4000/userlandexec) implementation.

# syscalls implementation

In cryload I use [musl-libc](https://www.musl-libc.org/) syscalls implementation.