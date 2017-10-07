# !bin/bash
echo "[Target-i386] GEN: bc_target_i386_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_i386_helpers.bc \
i386-softmmu/

echo "[Target-i386] GEN: bc_target_x86_64_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_x86_64_helpers.bc \
x86_64-softmmu/

## TODO: enable optimizations (such as -O2)
## In order to force not inline, using -O0 for now
echo "[Target-i386] GEN: bc_crete_ops.bc" & clang-3.4  \
    -c                                                 \
    -O0                                                \
    -fno-inline                                        \
    -emit-llvm                                         \
    tcg-llvm-offline/bc_crete_ops.c                    \
    -o bc_crete_ops.bc
