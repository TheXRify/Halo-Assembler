var: def "Hello, World!"
len: def 13
sp var
dp len
lie 0
eb 

loop:
lda %sp
sb %a
inc %sp
xor %sp, %dp
jz end
jmp loop

end:
db 
hlt 