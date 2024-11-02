; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"

@VERSION = global i32 42, align 4
@0 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@1 = private unnamed_addr constant [12 x i8] c"Value: %s\0A\0A\00", align 1
@2 = private unnamed_addr constant [12 x i8] c"Value: %d\0A\0A\00", align 1
@3 = private unnamed_addr constant [12 x i8] c"Value: %d\0A\0A\00", align 1

declare i32 @printf(ptr, ...)

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  %x1 = alloca ptr, align 8
  store ptr @0, ptr %x1, align 8
  %x2 = load ptr, ptr %x1, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @1, ptr %x2)
  %x3 = load i32, ptr %x, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @2, i32 %x3)
  store i32 100, ptr %x, align 4
  %x4 = load i32, ptr %x, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @3, i32 %x4)
  ret i32 0
}
