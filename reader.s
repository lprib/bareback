.global Parse_one
.global Parse

.equ	TAG_CONS, 0x0
.equ	TAG_INT, 0x1
.equ	TAG_SYMBOL, 0x2
.equ	TAG_NIL, 0x7

.section .bss
.balign	16
parse_heap:
	.zero 0x100
symbol_name_buf:
	.zero 0x100

.section .text

# heap* input* -> 
# sets up:
#	s1: heap iter
#	s2: input iter
Parse:
	addi	sp, sp, -12
	sw	ra, 8(sp)
	sw	s1, 4(sp)
	sw	s2, 0(sp)
	
	mv	s1, a0
	mv	s2, a1
loop_top:
	call	Parse_one
	lbu	t0, 0(s2)
	beqz	t0, eof
	j	loop_top
eof:
	lw	ra, 8(sp)
	lw	s1, 4(sp)
	lw	s2, 0(sp)
	addi	sp, sp, 12
	ret


Parse_one:
	addi	sp, sp, -12
	sw	ra, 8(sp)

	call	Skip_whitespace
	lbu	t0, 0(s2)
	beqz	t0, end_parse

	# t2: int accumulator
	# t3: found valid int?
	li	t2, 0
	li	t3, 0
parse_decimal:
	# if not 0-9, end digit collection
	lbu	t0, 0(s2)
	li	t1, '0'
	blt	t0, t1, store_decimal
	li	t1, '9'
	bgt	t0, t1, store_decimal
	li	t3, 1 # found an int
	
	slli	t4, t2, 3 # t4 = t2 * 8
	add	t4, t4, t2 # t4 = t2 * 9
	add	t4, t4, t2 # t4 = t2 * 10

	addi	t0, t0, -'0'
	add	t2, t0, t4 # prev*10+digit
	addi	s2, s2, 1
	j	parse_decimal
store_decimal:
	beqz	t3, parse_cons # did we find any digits or not
	slli	t2, t2, 2
	ori	t2, t2, TAG_INT
	sw	t2, 0(s1)
	addi	s1, s1, 4
	j	end_parse
parse_cons:
	lbu	t0, 0(s2)
	li	t1, '('
	bne	t0, t1, parse_symbol
	addi	s2, s2, 1
next_atom_in_cons:
	call	Skip_whitespace
	lbu	t0, 0(s2)
	beqz	t0, end_parse # EOF while in cons, ERROR (TODO signal this)
	li	t1, ')'
	bne	t0, t1, collect_car_cadr
	# In cons, found ')'. This is the base case, put NIL at top of stack
	# and quit loop. If this is the first value in a paren list, eg "()",
	# this just inserts NIL. If we are in a cons list "(a b c)", the c CONS
	# will point to the current top, which will now be NIL to signal end of
	# list
	addi	s2, s2, 1 # skip ')'
	li	t0, TAG_NIL
	sw	t0, 0(s1)
	addi	s1, s1, 4
	j	end_parse

collect_car_cadr:
	# In cons list with real values.
	# CAR contents: make pointer to 2 cells in the future (top hold this
	# CAR, next holds CADR). We want to point to AFTER that.
	addi	t0, s1, 8
	sw	t0, 0(s1)
	addi	s1, s1, 4
	sw	s1, 4(sp) # spill CADR ptr
	addi	s1, s1, 4 # reserve space for CADR

	# collect inner value
	call	Parse_one
	# fixup CADR to point to whatever the next value will be (either next
	# value in const list or NIL if ')' encountered
	lw	t0, 4(sp) # load previously spilled CADR ptr
	sw	s1, 0(t0) # store next index to CADR
	j	next_atom_in_cons

parse_symbol:
	# t0:	input char
	# t1:	atombuf iter
	la	t1, symbol_name_buf
next_char:
	lbu	t0, 0(s2)
	sb	t0, 0(t1)
	beqz	t0, intern_atom
	li	t2, ' '
	beq	t0, t2, intern_atom
	addi	t1, t1, 1
	addi	s2, s2, 1
	j	next_char
intern_atom:
	# no atom content, skip
	la	t2, symbol_name_buf
	beq	t1, t2, end_parse
	# null term the symbol name in buffer
	li	t0, 0
	sb	t0, 0(t1)

	la	a0, symbol_name_buf
	call	Intern_symbol
	sw	a0, 0(s1)
	addi	s1, s1, 4
end_parse:
	lw	ra, 8(sp)
	addi	sp, sp, 12
	ret

Skip_whitespace:
	lbu	t0, 0(s2)
	li	t1, ' '
	beq	t0, t1, 1f
	li	t1, '\t'
	beq	t0, t1, 1f
	li	t1, '\n'
	beq	t0, t1, 1f
	li	t1, '\r'
	beq	t0, t1, 1f
	ret
1:
	addi	s2, s2, 1
	j	Skip_whitespace

# vim: noet ts=8 sw=8 :
