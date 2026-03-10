"""
Python impl of lisp reader. Uses diferent data representation as asm.
"""
from intern import intern, unintern

TAG_MASK = 0b11
TAG_SYMB = 0b10
TAG_CONS = 0b00
TAG_INT  = 0b01
TAG_NIL  = 0b111

g_heap = [0 for _ in range(256)]
g_idx = 0

g_input = ""
g_ptr = 0

g_temp = list("\0" * 256)

def parse_one():
    global g_int
    global g_idx
    global g_heap
    global g_ptr

    c = g_input[g_ptr]
    if c in "0123456789":
        # INT
        the_int = 0
        while True:
            c = g_input[g_ptr]
            if c in "0123456789":
                the_int *= 10
                the_int += ord(c) - ord('0')
            else:
                break
            g_ptr += 1
        g_heap[g_idx] = (the_int << 2) | TAG_INT
        g_idx += 1
    elif c == "(":
        # CONS
        g_ptr += 1
        while True:
            skip_whitespace()
            if g_input[g_ptr] == ")":
                # Base case: closing paren puts a NIL ref at top of parse stack
                g_ptr += 1
                g_heap[g_idx] = TAG_NIL
                g_idx += 1
                break
            if g_input[g_ptr] == "\0":
                raise Exception("Expected closing paren")

            # generate CONS who's CAR points to the next upcoming value
            # Because CAR is always the next value to be parsed at read time
            g_heap[g_idx] = (g_idx + 2) << 2
            g_idx += 1
            cadr_idx = g_idx
            # Reserve space for the CADR pointer
            g_heap[g_idx] = 0
            g_idx += 1

            # now recursively parse one
            parse_one()

            # Fix up the CADR to point to what ever we will parse NEXT (will be
            # nil if closing paren)
            g_heap[cadr_idx] = g_idx << 2


    else:
        # SYMB
        i = 0
        while True:
            c = g_input[g_ptr]
            if c == '\0' or c == " ":
                break
            g_temp[i] = c
            i += 1
            g_ptr += 1

        g_temp[i] = '\0'
        if i != 0:
            id = intern(g_temp)
            g_heap[g_idx] = id
            g_idx += 1

def skip_whitespace():
    global g_ptr
    while g_input[g_ptr] == ' ':
        g_ptr += 1

def pretty_print(i, in_list=False):
    if (g_heap[i] & TAG_MASK) == TAG_SYMB:
        print(unintern(g_heap[i]), end="")
    elif (g_heap[i] & TAG_MASK) == TAG_CONS:
        car = g_heap[i] >> 2
        cadr = g_heap[i + 1] >> 2
        if in_list:
            print(" ", end="")
        else:
            print("(", end="")
        # CAR is not in the middle of a list, it's a pointer to a brand new
        # object
        pretty_print(car, in_list=False)
        pretty_print(cadr, in_list=True)
    elif (g_heap[i] & TAG_MASK) == TAG_INT:
        print(f"{g_heap[i] >> 2}", end="")
    elif (g_heap[i] & 0b111) == TAG_NIL:
        if in_list:
            print(")", end="")
        else:
            print("nil")


def dump_parse_tree():
    print("PARSE BUFFER:")
    i = 0
    for n in enumerate(g_heap):
        if i >= len(g_heap):
            break
        n = g_heap[i]
        tag = n & TAG_MASK
        if tag == TAG_SYMB:
            t = "SYMB"
            v = f"{unintern(n)} ({n >> 2})"
        elif tag == TAG_CONS:
            if (g_heap[i] == 0) and (g_heap[i+1] == 0):
                break
            t = "CONS"
            v = f"car={n >> 2}"
            if (i + 1) < len(g_heap):
                v += f" cadr={g_heap[i+1] >> 2}"
        elif tag == TAG_INT:
            t = "INT"
            v = n >> 2
        else:
            t = "SPCL"
            v = "nil" if g_heap[i] == 0b111 else ""
        print(f"{i:<2} {t:<4} {v}")
        i += 2 if tag == TAG_CONS else 1

def parse(string):
    global g_input
    g_input = string + "\0"
    while True:
        skip_whitespace()
        if g_input[g_ptr] == "\0":
            break

        parse_one()
        if g_input[g_ptr] == "\0":
            break

def car(idx):
    if (g_heap[idx] & TAG_MASK) != TAG_CONS:
        raise Exception("car on non-cons")
    return g_heap[idx]>>2

def cadr(idx):
    if (g_heap[idx] & TAG_MASK) != TAG_CONS:
        raise Exception("car on non-cons")
    return g_heap[idx+1]>>2

if __name__ == "__main__":
    parse("(1 2 3)")
    dump_parse_tree()
    pretty_print(cadr(0))
    print()
