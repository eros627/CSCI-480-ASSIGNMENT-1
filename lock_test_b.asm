acquirelocki 0
movi r1, #0
mapsharedmem r1, r2
movmr r3, r2
addi r3, #1
movrm r2, r3
printr r3
releaselocki 0
exit
