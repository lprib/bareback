/**
 * Test helpers
 */

#include "nostd.h"

static inline int strlen(char const* str)
{
    int len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

static inline void print_str(char const* str) {
    int len = strlen(str);
    write(1, str, len);
}

static inline void sprint_decimal(unsigned value, char* buf, int len)
{
    char tmp[10];
    int  i = 0, j = 0;

    if (!buf || len <= 0) return;

    if (value == 0) {
        buf[0] = '0';
        if (len > 1) buf[1] = '\0';
        else         buf[0] = '\0'; /* no room even for '0' */
        return;
    }

    while (value > 0 && i < 10) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0 && j < len - 1)
        buf[j++] = tmp[--i];

    buf[j] = '\0';
}

static inline void print_decimal(unsigned int n) {
    char buf[12];
    sprint_decimal(n, buf, sizeof(buf));
    print_str(buf);
}

static inline void sprint_hex(unsigned value, char* buf, int len)
{
    static const char hex[] = "0123456789abcdef";
    int i;
    if (!buf || len < 9) return;  /* need 8 digits + null */
    for (i = 0; i < 8; i++)
        buf[i] = hex[(value >> (28 - i * 4)) & 0xF];
    buf[8] = '\0';
}

static inline void print_hex(unsigned int n) {
    char buf[12];
    sprint_hex(n, buf, sizeof(buf));
    print_str(buf);
}

static inline void assert_eq(unsigned int a, unsigned int b) {
    if (a != b) {
        print_str("Assertion failed: a, b: 0x");
        print_hex(a);
        print_str(", 0x");
        print_hex(b);
        print_str("\n");
        exit(1);
    }
}

