/**
 * Assembly framework for building no-libc binaries.
 * Includes bare essential syscalls, and allows you to implement
 *
 *      int nostd_main(void)
 *
 * Which will be called on application startup.
 */

extern int nostd_main(void);

extern int write(int fd, char const* buf, int len);
extern void exit(int code);
