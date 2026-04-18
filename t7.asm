movi r1, #256
alloc r1, r2
cmpi r2, #0
jei #48
movi r3, #55
movrm r2, r3
freememory r2
alloc r1, r4
cmpi r4, #0
jei #24
movi r3, #66
movrm r4, r3
movmr r5, r4
printr r5
freememory r4
exit
movi r5, #0
printr r5
exit
