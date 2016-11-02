; TO BE REMOVED
; ModuleID = 'main_function.ll'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.CPUX86State = type opaque

; @tcg_env: should be inserted here

@env = external global %struct.CPUX86State*

define i32 @main() nounwind uwtable {
  %tcg_env_add = alloca i64, align 8
  %ptr_arg = alloca i64*, align 8
  %ret = alloca i64, align 8
  store %struct.CPUX86State* bitcast ([7128 x i64]* @tcg_env to %struct.CPUX86State*), %struct.CPUX86State** @env, align 8
  store i64 ptrtoint ([7128 x i64]* @tcg_env to i64), i64* %tcg_env_add, align 8
  store i64* %tcg_env_add, i64** %ptr_arg, align 8
  %1 = load i64** %ptr_arg, align 8

  ; call_tcg: %2

  store i64 %2, i64* %ret, align 8
  ret i32 0
}

; declare_tcg: declare i64 @tcg_llvm_tb_0(i64*)
