# !bin/bash
echo "[Target-i386] GEN: bc_target_i386_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_i386_helpers.bc \
i386-softmmu/

echo "[Target-i386] GEN: bc_target_x86_64_helpers.bc" & cp \
tcg-llvm-offline/op_helpers/bc_target_x86_64_helpers.bc \
x86_64-softmmu/
