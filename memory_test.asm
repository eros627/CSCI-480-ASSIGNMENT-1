; memory_test.asm
movi r3, #200     ; r3 = address 200
movi r2, #99      ; r2 = value 99
movrm r3, r2      ; [r3] = r2  -> mem[200] = 99
movmr r1, r3      ; r1 = [r3]  -> r1 = 99
printr r1
exit
