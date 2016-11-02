#ifndef CRETE_PRELOAD_H
#define CRETE_PRELOAD_H

extern "C" {
    // The type of __libc_start_main
    typedef int (*__libc_start_main_t)(
            int *(main) (int, char**, char**),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void (*stack_end)
            );

    int __libc_start_main(
            int *(main) (int, char **, char **),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void *stack_end)
            __attribute__ ((noreturn));
}

#endif // CRETE_PRELOAD_H
