movi r1, #100
alloc r1, r2
cmpi r2, #0
jei #24
movi r3, #42
movrm r2, r3
movmr r4, r2
printr r4
freememory r2
exit
movi r5, #0
printr r5
exit
