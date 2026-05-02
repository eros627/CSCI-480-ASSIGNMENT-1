movi r1, #256
alloc r1, r2
movi r3, #99
movrm r2, r3
movmr r4, r2
printr r4
freememory r2
alloc r1, r5
cmpi r5, #0
jei #12
printr r5
exit
movi r6, #0
printr r6
exit
