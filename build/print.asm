hp: def 100
maxhp: def 100
lvl: def 1

str: def "LOXDREW"
strlen: def 7
jmp menu

print_setup:
lie 0
sp str
dp strlen
eb 
jmp  print_loop

print_loop:
lda %sp
sb %a
inc %sp
xor %sp, %dp
jz menu2
jmp print_loop

menu2:
lda hp
sb %a
db 
hlt 

menu:
jmp print_setup