movi r1, #256
alloc r1, r2
cmpi r2, #0
jei #36
alloc r1, r3
cmpi r3, #0
jei #24
movi r4, #2
printr r4
freememory r2
freememory r3
exit
movi r4, #0
printr r4
exit
