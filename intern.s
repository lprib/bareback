.global Djb2_hash_str
.global Add_symbol
.global Intern_symbol
.global Symbol_table
.global Pre_intern_nil

.section .bss
.equ	MAX_SYMBOLS, 256
.equ	SYMBOL_ROW_SIZE, 12
.equ	SYMBOL_HASH, 0
.equ	SYMBOL_NAME, 4
.equ	SYMBOL_ID, 8

.equ	SYMBOL_TAG, 0x2
.equ	NIL_TAG, 0x7
Symbol_table:
.zero	(MAX_SYMBOLS*SYMBOL_ROW_SIZE)
symbol_names_blob:
.zero	0x1000

.section .data
symbol_names_next:
.word	symbol_names_blob
symbol_next_id:
.word	0
nil_symbol_name:
.asciz	"nil"

.section .text
# cstr -> int
Djb2_hash_str:
	#	a1=hash
	li	a1, 5381
1:
	lb	a2, 0(a0)
	beqz	a2, 2f
	add	a0, a0, 1
	slli	a3, a1, 5
	add	a1, a1, a3
	add	a1, a1, a2
	j	1b
2:
	mv	a0, a1
	ret

# cstr cstr -> int
# mustnt clobber tx
strcmp:
	lb	a2, 0(a0)
	lb	a3, 0(a1)
	beqz	a2, 1f
	bne	a2, a3, 1f
	add	a0, a0, 1
	add	a1, a1, 1
	j	strcmp
1:
	sub	a0, a2, a3
	ret

# cstr -> cstr
# appends new symbol to blob and returns pointer to it
Add_symbol:
	lw	t0, symbol_names_next
	mv	t2, t0
1:
	lbu	t1, 0(a0)
	sb	t1, 0(t0)
	addi	a0, a0, 1
	addi	t0, t0, 1
	beqz	t1, 2f
	j	1b
2:
	sw	t0, symbol_names_next, t1
	mv	a0, t2
	ret

# cstr -> int
Intern_symbol:
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s1, 8(sp)
	sw	s2, 4(sp)
	sw	s3, 0(sp)
	# hash test symbol
	mv	s1, a0
	call	Djb2_hash_str
	mv	s2, a0
	la	s3, Symbol_table
	# s1 = test str
	# s2 = test hash
	# s3 = symbol_table_iter
1:
	# ID = 0, end of registered symbols, need to freshly intern
	lw	t0, SYMBOL_ID(s3)
	beqz	t0, 3f
	# check hashes match
	lw	t0, SYMBOL_HASH(s3)
	bne	t0, s2, 2f
	# check names match
	mv	a0, s1
	lw	a1, SYMBOL_NAME(s3)
	call	strcmp
	bnez	a0, 2f
	# everything matched, return ID
	lw	a0, SYMBOL_ID(s3)
	j	4f
2:
	# next
	addi	s3, s3, SYMBOL_ROW_SIZE
	j	1b
3:
	# add symbol
	mv	a0, s1
	call	Add_symbol
	sw	a0, SYMBOL_NAME(s3)
	sw	s2, SYMBOL_HASH(s3)
	lw	a1, symbol_next_id
	slli	a0, a1, 2 # make space for tag
	ori	a0, a0, SYMBOL_TAG
	sw	a0, SYMBOL_ID(s3)
	addi	a1, a1, 1 # increment ID counter (without symbol tag)
	sw	a1, symbol_next_id, t0
4:
	# exit
	lw	ra, 12(sp)
	lw	s1, 8(sp)
	lw	s2, 4(sp)
	lw	s3, 0(sp)
	addi	sp, sp, 16
	ret

Pre_intern_nil:
	# nil has special id of 0x7fffffff
	addi	sp, sp, -4
	sw	ra, 0(sp)
	la	a0, nil_symbol_name
	call	Djb2_hash_str
	la	t0, Symbol_table
	sw	a0, SYMBOL_HASH(t0)
	la	a0, nil_symbol_name
	sw	a0, SYMBOL_NAME(t0)
	li	a0, NIL_TAG
	sw	a0, SYMBOL_ID(t0)
	lw	ra, 0(sp)
	addi	sp, sp, 4
	ret

# vim: noet ts=8 sw=8 :
