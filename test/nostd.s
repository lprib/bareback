/**
 * Basic entry point to no-libc executable (for test harness)
 */
.global write
.global exit
.global nostd_main
.global _start

.equ SYSCALL_OPENAT, 56
.equ SYSCALL_READ, 63
.equ SYSCALL_WRITE, 64
.equ SYSCALL_EXIT, 93

.equ AT_FDCWD, -100
.equ O_RDONLY, 0

.section .text
_start:
	call	nostd_main
	call	exit

write:
	li	a7, SYSCALL_WRITE
	ecall
	ret

exit:
	li	a7, SYSCALL_EXIT
	ecall
	ret

# vim: noet ts=8 sw=8 :
