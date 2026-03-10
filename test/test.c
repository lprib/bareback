/**
 * Test suite for assembly
 */

#include "util.h"

extern int Djb2_hash_str(char const*);
extern char const* Add_symbol(char const*);
extern void Pre_intern_nil(void);
extern unsigned Intern_symbol(char const*);
extern void Parse(int* heap, char const* input);

struct Symbol {
    unsigned hash;
    char const* name;
    unsigned id;
};
extern struct Symbol Symbol_table[256];

void dump_symbol_table(void) {
    for(int j = 0; j < 256; j++) {
        if (!Symbol_table[j].name)
            continue;
        print_hex(Symbol_table[j].id >> 2);
        print_str(" hash=");
        print_hex(Symbol_table[j].hash);
        print_str(" '");
        print_str(Symbol_table[j].name);
        print_str("'\n");
    }
}

static inline int is_cons(unsigned v)       { return (v & 0x3) == 0x0; }
static inline int is_int(unsigned v)        { return (v & 0x3) == 0x1; }
static inline int is_symbol(unsigned v)     { return (v & 0x3) == 0x2; }
static inline int is_special(unsigned v)    { return (v & 0x3) == 0x3; }
// special is 0b11 tag, nil is 0b111
static inline int is_nil(unsigned v)    { return (v & 0x7) == 0x7; }

static inline unsigned* car(unsigned* cons) {
    return (unsigned*)cons[0];
}
static inline unsigned* cadr(unsigned* cons) {
    return (unsigned*)cons[1];
}

void dump_heap(int* heap, unsigned size) {
    for (int i = 0; i < size; i++) {
        print_hex((unsigned)(&heap[i]));
        print_str(" ");
        print_hex(heap[i]);
        print_str(": ");
        if (is_int(heap[i])) {
            print_str("INT  ");
            print_decimal(heap[i] >> 2);
        } else if (is_symbol(heap[i])) {
            print_str("SYMB ");
            for(int j = 0; j < 256; j++) {
                if (Symbol_table[j].id == heap[i]) {
                    print_str(Symbol_table[j].name);
                    print_str(" (");
                    print_hex(Symbol_table[j].id >> 2);
                    print_str(")");
                    break;
                }
            }
        } else if (is_cons(heap[i])) {
            print_str("CONS car=");
            print_hex((unsigned)car(&heap[i]));
            print_str(" cadr=");
            print_hex((unsigned)cadr(&heap[i]));
            i++;
        } else if (is_special(heap[i])) {
            print_str("SPCL ");
            if (is_nil(heap[i]))
                print_str("nil");
        }
        print_str("\n");
    }
}

static inline int tagged_symbol(int idx) {
    return (idx << 2) | 0b10;
}

int nostd_main(void) {
    Pre_intern_nil();

    // test hash
    assert_eq(Djb2_hash_str("hello"), 0xF923099);

    // test interning
    assert_eq(Intern_symbol("hello"), tagged_symbol(0));
    assert_eq(Intern_symbol("hello"), tagged_symbol(0));
    assert_eq(Intern_symbol("there"), tagged_symbol(1));
    assert_eq(Intern_symbol("hello"), tagged_symbol(0));
    assert_eq(Intern_symbol("another"), tagged_symbol(2));
    assert_eq(Intern_symbol("there"), tagged_symbol(1));
    assert_eq(Intern_symbol("nil"), 0x7);

    // test parsing
    char const* in1 = "(+ 1 2)";
    int heap[32];
    for(int i = 0; i < 32; i++) heap[i] = 0;
    Parse(heap, in1);
    dump_heap(heap, 32);
    dump_symbol_table();

    print_str("OK\n");
    return 0;
}
