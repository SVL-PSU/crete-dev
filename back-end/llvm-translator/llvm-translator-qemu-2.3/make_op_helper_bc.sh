# !bin/bash
echo "[Target-i386] GEN: bc_target_i386_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_i386_helpers.bc \
i386-softmmu/

echo "[Target-x86_64] GEN: bc_target_x86_64_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_x86_64_helpers.bc \
x86_64-softmmu/

echo "GEN: bc_crete_ops.bc" & clang-3.4  \
    -c                                                 \
    -O2                                                \
    -emit-llvm                                         \
    tcg-llvm-offline/bc_crete_ops.c                    \
    -o bc_crete_ops.bc
