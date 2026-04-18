movi r1, #512
alloc r1, r2
cmpi r2, #0
jei #60
movi r3, #10
movrm r2, r3
movi r4, #256
addr r4, r2
movi r3, #20
movrm r4, r3
movmr r5, r2
printr r5
movmr r6, r4
printr r6
freememory r2
exit
movi r5, #0
printr r5
exit
