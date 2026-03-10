"""
Python implementation of string interning to model the ASM from.  Note this
uses a different data representation than the asm as the asm was changed and
couldne be fucked to update this.
"""
symbol_id_counter = 0

HASH = 0
NAME_PTR = 1
ID = 2
# list of "structs" which are 3-elem lists, indexed by HASH, NAME_PTR, ID
g_symbol_table = [[0, 0, 0] for _ in range(256)]

# blob of zero-terminated strings. NAME_PTR of above table indexes in to here
g_symbol_name_ptr = 0
g_symbol_names_blob = list("\0" * 256)

def djb2(s: str) -> int:
    hash_value = 5381
    i = 0
    while s[i] != '\0':
        hash_value = ((hash_value << 5) + hash_value + ord(s[i])) & 0xFFFFFFFF
        i += 1
    return hash_value

def streq(s1, s2) -> bool:
    i = 0
    while True:
        c1 = s1[i]
        c2 = s2[i]
        if c1 == '\0':
            break
        if c1 != c2:
            break
        i += 1
    return c1 == c2

def add_symbol(name):
    global g_symbol_name_ptr
    initial_symbol_name_index = g_symbol_name_ptr
    i = 0
    while True:
        g_symbol_names_blob[g_symbol_name_ptr] = name[i]
        if name[i] == '\0':
            break
        i += 1
        g_symbol_name_ptr += 1
    g_symbol_name_ptr += 1
    return initial_symbol_name_index


def intern(symbol_name):
    global symbol_id_counter
    symbol_hash = djb2(symbol_name)


    #print(bytes(symbol_name, encoding="utf-8"))
    diag_str =  "".join(symbol_name[:symbol_name.index('\0')])
    print(f"intern: '{diag_str}'")

    i = 0
    while True:
        print(f"intern: check i={i}")
        lu_hash = g_symbol_table[i][HASH]
        lu_symbol_ptr = g_symbol_table[i][NAME_PTR]
        lu_str = g_symbol_names_blob[lu_symbol_ptr:]
        lu_id = g_symbol_table[i][ID]
        
        print(f"intern: {lu_hash=} {lu_symbol_ptr=} {lu_id=}")

        # 0 not a valid intern ID
        if lu_id == 0:
            break

        if lu_hash == symbol_hash:
            print(f"intern: hash match")
            if streq(symbol_name, lu_str):
                print(f"intern: string match")
                return lu_id

        i += 1

    # end of table, insert @ i
    print(f"intern: inserting at {i=}")
    g_symbol_table[i][HASH] = symbol_hash # should be able to get existing pointers from loop here
    new_symbol_name_index = add_symbol(symbol_name)
    g_symbol_table[i][NAME_PTR] = new_symbol_name_index
    this_symbol_id = (symbol_id_counter << 2) | 0b10
    g_symbol_table[i][ID] = this_symbol_id
    symbol_id_counter += 1
    return this_symbol_id

def unintern(id):
    for row in g_symbol_table:
        r_ptr = row[NAME_PTR]
        if row[ID] == id:
            return "".join(g_symbol_names_blob[r_ptr:g_symbol_names_blob.index('\0', r_ptr)])
    return "<NOTFOUND>"

def cstr(s):
    return f"{s}\x00"
    
def test_streq(a, b):
    streq_result = streq(cstr(a), cstr(b))
    assert (streq_result == (a == b))

def dump_symbol_table():
    for row in g_symbol_table:
        if row[ID] == 0:
            break
        r_hash = row[HASH]
        r_ptr = row[NAME_PTR]
        r_id = row[ID]
        r_str = "".join(g_symbol_names_blob[r_ptr:g_symbol_names_blob.index('\0', r_ptr)])
        print(f"{r_hash:<10} {r_id>>2} ({r_id}) {r_str:<10}")

if __name__ == "__main__":
    #test_streq("hello", "world")
    #test_streq("hello", "hello")
    #test_streq("a", "ab")
    #test_streq("ab", "a")
    #test_streq("a", "a")

    print(hex(intern(cstr("abc"))))
    print(hex(intern(cstr("def"))))
    print(hex(intern(cstr("abc"))))
    dump_symbol_table()
