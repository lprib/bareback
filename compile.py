#source = [ "+", 1, [ "*", 2, 3 ] ]

# Semantics for a simple recursive compiler.
# The implicit calling convention is
# args in a0...
# return in a0
# 
# Callee is NOT allowed to clobber any more ax registers than are passed in
# args. i.e. if function takes 2 params, it can 

def compile(form, argi, form_is_head):
    #print(f"    compile {form=} {argi=} {form_is_head=}")
    if isinstance(form, (list, slice)) and len(form) > 0:
        if form_is_head:
            if form[0] == "quote":
                # NOTE quote can only have one arg
                print(f"FOLLOW CAR {form[1]} -> a{argi}")
            else:
                # Head and list, do function application
                if argi > 0:
                    print(f"SPILL up to a{argi-1}")
                compile(form[1:], 0, False)
                print(f"CALL {form[0]}, mv a0 -> a{argi}")
                if argi > 0:
                    print(f"FILL up to a{argi-1}")
        else:
            compile(form[0], argi, True) # It is head or atom here
            compile(form[1:], argi+1, False) # iterate
    if isinstance(form, (list, slice)) and len(form) == 0:
        # NIL
        if form_is_head:
            print("NIL -> a{argi}")
        else:
            pass # Base case
    elif isinstance(form, int):
        if argi == -1:
            argi = 0
        print(f"{form} -> a{argi}")
    elif form is None:
        pass

def sexp(x):
    return f"({' '.join(sexp(i) for i in x)})" if isinstance(x, list) else str(x)

def print_compile(source):
    print(f"========================================")
    print(f"program: {sexp(source)}")
    compile(source, 0, True)

print_compile(["set", ["quote", "my-var"], 4])
print_compile(["+", ["/", 22, 33], ["*", 99, 66] , 3])
print_compile(["lambda", ["quote", ["x", "y"]], ["quote", ["+", "x", "y"]]])
