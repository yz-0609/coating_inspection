#include "bsp_UART.h"
#include "main.h"
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
int _write(int file,char *buffer,int length)
{
    if (file!=1 && file!=2) { errno=EBADF; return -1; }
    if (length<0) { errno=EINVAL; return -1; }
    if (!BSP_DebugWrite(buffer,(size_t)length)) { errno=EAGAIN; return -1; }
    return length;
}
void *_sbrk(ptrdiff_t increment)
{
    extern unsigned char __heap_start__,__heap_end__;
    static unsigned char *current;
    if (!current) current=&__heap_start__;
    if ((increment>0 && (uintptr_t)increment>(uintptr_t)(&__heap_end__-current)) ||
        (increment<0 && (uintptr_t)(-(increment+1))+1U>(uintptr_t)(current-&__heap_start__))) {
        errno=ENOMEM; return (void *)-1;
    }
    unsigned char *previous=current; current+=increment; return previous;
}
int _close(int fd) { (void)fd; errno=EBADF; return -1; }
int _fstat(int fd,struct stat *s) { if (fd<0 || fd>2) { errno=EBADF; return -1; } s->st_mode=S_IFCHR; return 0; }
int _isatty(int fd) { return fd>=0 && fd<=2; }
int _lseek(int fd,int offset,int whence) { (void)fd; (void)offset; (void)whence; errno=ESPIPE; return -1; }
int _read(int fd,char *b,int n) { (void)fd; (void)b; (void)n; errno=ENOSYS; return -1; }
int _getpid(void) { return 1; }
int _kill(int pid,int signal) { (void)pid; (void)signal; errno=EINVAL; return -1; }
void _exit(int status) { (void)status; Error_Handler(); for (;;) {} }
void _init(void) { setvbuf(stdout,NULL,_IONBF,0); setvbuf(stderr,NULL,_IONBF,0); }
void _fini(void) {}
