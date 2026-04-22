.global Compile

.equ	TAG_CONS, 0x0
.equ	TAG_INT, 0x1
.equ	TAG_SYMBOL, 0x2
.equ	TAG_NIL, 0x00000002


.section .bss
.balign 16
# (int symbol_id, int* heap_value)[0x100]
Global_bindings:
.zero 0x200

.section .text


## DONT TRUST THESE COMMENTS.... ALGORITYM IS HASHED OUT IN COMPILE.PY

# destination should be a saved reg or global
# form*, arg_idx
Compile:
	addi	sp, sp, -12
	sw	ra, 8(sp)
	sw	s0, 4(sp)
	sw	s1, 0(sp)

	lw	t0, 0(a0)
	andi	t0, t0, 0x3
	li	t1, 0x0
	beq	t0, t1, cons
	li	t1, 0x1
	beq	t0, t1, int
	li	t1, 0x2
	beq	t0, t1, symbol
	li	t1, 0x3
	beq	t0, t1, special
	# unreachable

cons:
	# If arg_idx == -1, this is the function of an application.
	#	Look up CAR in Global bindings, cache it in saved or stack.
	#	Compile(CDR(form), arg_idx+1)
	#	Compile a CALL instruction along with save/restore to the
	#	cached CAR function value
	# 
	# if arg_idx >= 0, this is a member of a function application
	#	Compile(CAR(form), arg_idx) --> This will fall through to int case and actually compile the load isntr
	#	Compile(CDR(form), arg_idx+1) --> This will end up back in this case, but with arg_idx+1

	j	exit

	# Dest is where the code will live. For non-cons (not funciton
	# application), at dest location, compile a load to the address
	# register corresponding to arg_idx of the form at form*.
int:
	# If form* is integer, simply lw the int to register ax. Ax corresponds
	# to argument register X (which is arg_idx*2 since args can be 2 regs
	# large in case of cons)
	j	exit
symbol:
	# (TODO when lambdas), look up the symbol in the local bindings stack.
	# Load the symbol's value to the ax register (pass by copy)
	j	exit
special:
	# If nil, this is the end of the arg list, exit out
	j	exit
exit:

	lw	ra, 8(sp)
	lw	s0, 4(sp)
	lw	s1, 0(sp)
	addi	sp, sp, 12
	




# vim: noet ts=8 sw=8 :
