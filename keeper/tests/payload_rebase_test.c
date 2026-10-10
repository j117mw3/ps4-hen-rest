#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
/* Execute only the starter's BSS initialization and pointer guard. Both SDK
 * initializers are stubbed to RET in a private memory copy, so no Orbis syscall
 * or console interaction is possible. Multiple bases reproduce the first
 * captured fault, which used absolute BSS addresses after rebasing. */
int main(int argc, char **argv)
{
    assert(argc == 6);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long length = ftell(f); rewind(f);
    assert(length > 0); unsigned char *binary = malloc((size_t)length); assert(binary);
    assert(fread(binary, 1, (size_t)length, f) == (size_t)length); fclose(f);
    size_t kernel = strtoull(argv[2], NULL, 16), libc = strtoull(argv[3], NULL, 16);
    size_t begin = strtoull(argv[4], NULL, 16), end = strtoull(argv[5], NULL, 16);
    assert(kernel < (size_t)length && libc < (size_t)length && begin < end && end <= (size_t)length);
    uintptr_t bases[] = {0x30000000ULL, 0x100000000ULL, 0};
    for (unsigned i = 0; i < sizeof(bases)/sizeof(bases[0]); ++i) {
        pid_t child = fork(); assert(child >= 0);
        if (!child) {
            size_t capacity = ((size_t)length + 4095) & ~(size_t)4095;
            int flags = MAP_PRIVATE | MAP_ANONYMOUS;
            if (bases[i]) flags |= MAP_FIXED_NOREPLACE;
            unsigned char *copy = mmap((void *)bases[i], capacity, PROT_READ|PROT_WRITE|PROT_EXEC, flags, -1, 0);
            assert(copy != MAP_FAILED);
            memcpy(copy, binary, (size_t)length);
            copy[kernel] = copy[libc] = 0xc3;
            memset(copy + begin, 0xa5, end - begin);
            int (*entry)(void); memcpy(&entry, &copy, sizeof(entry));
            assert(entry() == -1); /* Guard returns before any SDK operation. */
            for (size_t n = begin; n < end; ++n) assert(copy[n] == 0);
            munmap(copy, capacity); _exit(0);
        }
        int status; assert(waitpid(child, &status, 0) == child);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            fprintf(stderr, "FAIL: relocated flat payload at base 0x%lx, wait status=%d\n", (unsigned long)bases[i], status);
            return 1;
        }
    }
    free(binary);
    puts("PASS: flat payload startup/BSS guard executes at three different bases; SDK calls stubbed");
}
