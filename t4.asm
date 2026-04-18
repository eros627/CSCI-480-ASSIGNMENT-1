movi r1, #1
alloc r1, r2
cmpi r2, #0
jei #48
movi r3, #252
addr r3, r2
movi r4, #77
movrm r3, r4
movmr r5, r3
printr r5
freememory r2
exit
movi r5, #0
printr r5
exit
