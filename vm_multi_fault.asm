movi r1, #256
alloc r1, r2
movi r3, #77
movrm r2, r3
movi r1, #256
alloc r1, r4
movi r3, #88
movrm r4, r3
movmr r5, r2
printr r5
movmr r6, r4
printr r6
freememory r2
freememory r4
exit
