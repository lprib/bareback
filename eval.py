"""
Evaluate in-memory forms. Need to decide how we will do call stack, for now
using the python call stack which isn't translatatble to asm
"""
from reader import *
from intern import dump_symbol_table

g_stack = [0 for _ in range(256)]
g_sp = 256

def push_int(n):
    global g_stack
    global g_sp

    g_sp -= 1
    g_stack[g_sp] = (n << 2) | TAG_INT

def plus(iform):
    acc = 0
    while True:
        iform = cadr(iform)
        acc += eval_form(car(iform)) >> 2
        if (g_heap[cadr(iform)] & 0b111) == TAG_NIL:
            break
    return (acc << 2) | TAG_INT

def minus(iform):
    iform = cadr(iform)
    acc = eval_form(car(iform)) >> 2
    while True:
        iform = cadr(iform)
        acc -= eval_form(car(iform)) >> 2
        if (g_heap[cadr(iform)] & 0b111) == TAG_NIL:
            break
    return (acc << 2) | TAG_INT


g_bindings = {
    intern("+\0"): plus,
    intern("-\0"): minus,
}

def eval_form(iform) -> int:
    if (g_heap[iform] & TAG_MASK) == TAG_CONS:
        fn = g_bindings[g_heap[car(iform)]]
        return fn(iform)
    else:
        return g_heap[iform]


if __name__ == "__main__":
    parse("(+ 10 (- 7 2))")
    dump_symbol_table()
    dump_parse_tree()
    print(g_bindings)
    print(eval_form(0) >> 2)
