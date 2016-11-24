# !bin/bash

### Note:
# 1. run this script under i386-softmmu
# 2. use -O0 -fno-inline for debugging

echo "  LLVMCC    bc_i386_helpers.bc" && clang      \
    -c                                              \
    -O0 -fno-inline                                 \
    -I./ 	                                    \
    -I../ 	                                    \
    -I../include 	                            \
    -I../tcg/ 	                                    \
    -I../tcg/i386 	                            \
    -I../target-i386 	                            \
    -I../linux-headers 	                            \
    -I../include/qemu/ 	                            \
    -I../include/exec/ 	                            \
    -I/usr/include/libpng12 	                    \
    -I/usr/include/glib-2.0 	                    \
    -I/usr/include/p11-kit-1 	                    \
    -I/usr/include/libpng12  	                    \
    -I/usr/lib/x86_64-linux-gnu/glib-2.0/include    \
    \
    -DNEED_CPU_H                                    \
    -emit-llvm                                      \
    ../target-i386/bc_i386_helpers.c                \
    -o bc_i386_helpers.bc

    # -O2                                             \


### old flags
    # -DPIE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAS_AUDIO -DHAS_AUDIO_CHOICE -DTARGET_PHYS_ADDR_BITS=64 \
    # -Wno-invalid-noreturn
    # -m64
    # -MMD -MP -MT -march=native  \
