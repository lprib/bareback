#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define DBG_ENABLED 0
#define DBG(_stmt) if(DBG_ENABLED) {_stmt;}

#define HEAPALIGN 0x8 // 3byte tag = 8 byte align

#define TAGMASK 0b111
#define FIXEVEN 0b000 // b0:1 = 0 => FIX
#define UNUSED1 0b001 // b1 = 0 => IMMEDIATE TYPE
#define CONS    0b010 // b1 = 1 => POINTER TYPE
#define SYMBOL  0b011 // b1 = 1 => POINTER TYPE
#define FIXODD  0b100 // b0:1 = 0 => FIX
#define UNUSED2 0b101 // b1 = 0 => IMMEDIATE TYPE
#define HEAPPTR 0b110 // b1 = 1 => POINTER TYPE
#define UNUSED3 0b111 // b1 = 1 => POINTER TYPE

#define HHMASK  0xff
#define HHCLOS  0x00
#define HHPRIM  0x01
#define HHBUF   0x02
#define HHSTR   0x03

typedef long cell;
typedef cell (*primitivefn) (cell args);
struct cons { cell car, cdr; };
struct primitive { cell hh; primitivefn fn; };
struct closure { cell hh, argnames, body, env; };
struct buffer { cell hh; char data[]; };
#define istype(_cell, _type) (((_cell) & TAGMASK) == (_type))
#define isfix(_cell) (istype((_cell), FIXEVEN) || istype((_cell), FIXODD))
#define asptr(_cell) ((_cell) & ~TAGMASK)
#define isptrtype(_cell) (((_cell) & 0b010) != 0)
#define ishhptrtype(_cell) (isptrtype(_cell) && !istype(_cell, CONS))
cell internlist; // lisp list of symbols that are intened
cell nil; // nil symbol
cell globalenv; // global environment lisp alist

#define HEAPSIZE 0x100000
cell* heapa; // Underlying allocation A
cell* heapb; // Underlying allocation B
cell* heap; // Current heap (will A/B swap during GC)
cell* heaptop;
cell* toheap; // Heap to collect into during GC, will A/B swap during GC
cell* toheaptop;
// Used to protect stack variables during GC (so GC can move them)
struct gcframe {
  struct gcframe* parent;
  int cnt;
  cell** roots;
};
struct gcframe* topgcframe = NULL;
// Pass in refs to local variables so that the GC knowns about them and can move
// them. GCPROTECT(&myvar); ...; gc(); means myvar will be edited by gc process
// to move to new heap
#define GCPROTECT(...) \
  cell* _roots[] = { __VA_ARGS__ }; \
  struct gcframe _frame = { topgcframe, sizeof(_roots)/sizeof(_roots[0]), _roots }; \
  topgcframe = &_frame;
// Don't bother protecting vars in this stack frame anymore
#define ENDGCPROTECT() topgcframe = _frame.parent;
// Bitmap of whether this value has been forwarded to new heap during GC. One
// bit per aligned heap slot.
unsigned int forwardmap[HEAPSIZE / HEAPALIGN / 32] = {0};
// stack of values who's children need to be reachability searched
cell* gcworkstack[0x1000];
cell** gcworkstacktop = gcworkstack;

void println(cell c);
void printexpr(cell c);


// ALLOC & GC

void* alloc(int cells, cell** whichheaptop) {
  cell* p = *whichheaptop;
  *whichheaptop = (cell*)(((cell)(*whichheaptop + cells) + HEAPALIGN - 1) & ~(HEAPALIGN - 1));
  return p;
}

char* allocbytes(int bytes, cell** whichheaptop) {
  return alloc((bytes + sizeof(cell) - 1) / sizeof(cell), whichheaptop);
}

int isforwarded(cell* inheap) {
  int slot = ((void*)inheap - (void*)heap) / HEAPALIGN;
  return (forwardmap[slot/32] & (1u << (slot%32))) != 0;
}

int isinmainheap(void* c) {
  return (c >= (void*)heap) && (c < ((void*)heap + HEAPSIZE));
}

void markforwarded(cell* inheap, void* to) {
  *inheap = (cell)(to);
  int slot = ((void*)inheap - (void*)heap) / HEAPALIGN;
  forwardmap[slot/32] |= (1u << (slot%32));
}

void moveimm(cell* imm) {
  if (!isptrtype(*imm))
    return;

  cell* pointer = (cell*)asptr(*imm);
  if (isforwarded(pointer)) {
    DBG(printf("moveimm already fwd ")); DBG(printexpr(*imm)); DBG(printf("\n"));
    // (*imm) is a pointer immediate that points to old heap location. That old
    // heap location contains a pointer to it's new residence in the toheap
    *imm = (*imm & TAGMASK) | *pointer; // replace imm with forward pointer
    return;
  }

  DBG(printf("moveimm ")); DBG(printexpr(*imm)); DBG(printf("\n"));
  if (istype(*imm, CONS)) {
    struct cons* oldcons = (struct cons*)asptr(*imm);
    struct cons* newcons = alloc(sizeof(struct cons)/sizeof(cell), &toheaptop);
    newcons->car = oldcons->car;
    newcons->cdr = oldcons->cdr;
    *gcworkstacktop++ = &newcons->cdr;
    *gcworkstacktop++ = &newcons->car;
    markforwarded(pointer, newcons);
    *imm = (*imm & TAGMASK) | (cell)(newcons);
  } else if (isptrtype(*imm)) {
    cell stag = *(cell*)asptr(*imm);
    int lencells = ((stag >> 8) + sizeof(cell) - 1) / sizeof(cell);
    cell* new = alloc(lencells + 1, &toheaptop);
    memcpy(new, pointer, (lencells + 1)*sizeof(cell));
    markforwarded(pointer, new);
    *imm = (*imm & TAGMASK) | (cell)new;
    if ((stag & HHMASK) == HHCLOS) {
      *gcworkstacktop++ = &((struct closure*)new)->argnames;
      *gcworkstacktop++ = &((struct closure*)new)->body;
      *gcworkstacktop++ = &((struct closure*)new)->env;
    }
  }
}

void gcframe(struct gcframe* frame) {
  if (!frame) return;
  for (int i = 0; i < frame->cnt; i++) {
    cell* root = frame->roots[i];
    *gcworkstacktop++ = root;
    while (gcworkstacktop > gcworkstack)
      moveimm(*(--gcworkstacktop));
  }
  gcframe(frame->parent);
}

void gc(void) {
  DBG(printf("GC INVOKED\n"));
  gcframe(topgcframe);
  heap = (heap == heapa) ? heapb : heapa;
  toheap = (heap == heapa) ? heapb : heapa;
  heaptop = toheaptop; // start allocating where GC left off
  toheaptop = toheap; // discard everything in toheap
  memset(forwardmap, 0, sizeof(forwardmap));
}


// METACIRCULAR EVALUATOR BOOTSTRAP

cell cons(cell car, cell cdr) {
  struct cons* cns = alloc(sizeof(struct cons)/sizeof(cell), &heaptop);
  cns->car = car;
  cns->cdr = cdr;
  return ((cell)cns) | CONS;
}

cell car(cell cons) {
  if (cons == nil) return nil;
  assert(istype(cons, CONS));
  return ((struct cons*)asptr(cons))->car;
}

cell cdr(cell cons) {
  if (cons == nil) return nil;
  assert(istype(cons, CONS));
  return ((struct cons*)asptr(cons))->cdr;
}

#define cadr(_cons) car(cdr(_cons))
#define fix(_n) ((cell)((_n) << 2))
#define getfix(_n) ((_n) >> 2)

cell buffer(int lenbytes) {
  int capcells = (lenbytes + sizeof(cell) - 1) / sizeof(cell);
  struct buffer* buf = alloc(sizeof(struct buffer)/sizeof(cell) + capcells, &heaptop);
  assert(lenbytes < (1<<24));
  buf->hh = lenbytes << 8 | HHBUF;
  return (cell)buf | HEAPPTR;
}

#define strc(s) str(s, strlen(s))
cell str(char* s, int strlen) {
  cell bufcell = buffer(strlen+1);
  struct buffer* buf = (struct buffer*)asptr(bufcell);
  memcpy(buf->data, s, strlen);
  buf->data[strlen] = '\0';
  buf->hh = (buf->hh & ~HHMASK) | HHSTR;
  return bufcell;
}

#define symc(s) sym(s, strlen(s))
cell sym(char* s, int strlen) { return asptr(str(s, strlen)) | SYMBOL; }

struct bufinner { char* data; int len; };
struct bufinner getstr(cell bufcell) {
  assert(ishhptrtype(bufcell));
  struct buffer* buf = (struct buffer*)asptr(bufcell);
  assert(((buf->hh & HHMASK) == HHBUF) || ((buf->hh & HHMASK) == HHSTR));
  return (struct bufinner){ buf->data, buf->hh >> 8 };
}

#define internc(s) intern(s, strlen(s))
cell intern(char* s, int len) {
  cell rest = internlist;
  while (rest != nil) {
    char* name = getstr(car(rest)).data;
    if ((len == strlen(name)) && !memcmp(name, s, len))
      return car(rest);
    rest = cdr(rest);
  }
  internlist = cons(sym(s, len), internlist);
  return car(internlist);
}

cell prim(primitivefn fn) {
  struct primitive* primitive = alloc(sizeof(struct primitive) / sizeof(cell), &heaptop);
  primitive->hh = (sizeof(struct primitive) - sizeof(cell)) << 8 | HHPRIM;
  primitive->fn = fn;
  return (cell)primitive | HEAPPTR;
}

cell closure(cell argnames, cell body, cell env) {
  struct closure* closure = alloc(sizeof(struct closure)/sizeof(cell), &heaptop);
  closure->hh = (sizeof(struct closure) - sizeof(cell)) << 8 | HHCLOS;
  closure->argnames = argnames;
  closure->body = body;
  closure->env = env;
  return (cell)closure | HEAPPTR;
}

cell assoc(cell key, cell alist) {
  if (alist == nil)
    return nil;
  if (key == car(car(alist)))
    return car(alist);
  return assoc(key, cdr(alist));
}

cell envlookup(cell sym, cell env) {
  if (sym == nil) return nil;
  if (sym == internc("$"))
    return globalenv;
  if (sym == internc("%"))
    return internlist;

  if (env == nil) {
    // Ideally we would treat global as just another env. But that breaks
    // recursive functions because the global environment they capture does not
    // yet contain themselves
    cell globalpair = assoc(sym, globalenv);
    if (globalpair != nil)
      return cdr(globalpair);
    printf("ERR unbound var: %s\n", getstr(sym).data);
    return nil;
  }

  cell localframepair = assoc(sym, car(env));
  if (localframepair != nil)
    return cdr(localframepair);
  return envlookup(sym, cdr(env));
}

cell pairlis(cell ks, cell vs) {
  if (ks == nil) return nil;
  return cons(cons(car(ks), car(vs)), pairlis(cdr(ks), cdr(vs)));
}

cell eval(cell expr, cell env);

// CALLERS MUST GCPROTECT THEIR LOCALS
cell evallist(cell list, cell env) {
  cell head = nil;
  cell res = nil;
  GCPROTECT(&list, &env, &head);

  if (list != nil) {
    head = eval(car(list), env);
    cell tail = evallist(cdr(list), env);
    res = cons(head, tail);
  }

  ENDGCPROTECT();
  return res;
}

// CALLERS MUST GCPROTECT THEIR LOCALS
cell evalcond(cell branches, cell env) {
  if (branches == nil) return nil;
  GCPROTECT(&branches, &env);
  cell result = nil;
  if (eval(car(car(branches)), env) != nil)
    result =  eval(cadr(car(branches)), env);
  else
    result = evalcond(cdr(branches), env);

  ENDGCPROTECT();
  return result;
}

// CALLERS MUST GCPROTECT THEIR LOCALS
cell progn(cell bodylist, cell env) {
  if (cdr(bodylist) == nil) {
    // base case requires no gc protect due to tail call
    return eval(car(bodylist), env);
  }

  GCPROTECT(&bodylist, &env);
  (void)eval(car(bodylist), env);
  ENDGCPROTECT();
  return progn(cdr(bodylist), env);
}

cell apply(cell proc, cell args);

// CALLERS MUST GCPROTECT THEIR LOCALS
cell eval(cell expr, cell env) {
  cell res = nil;

  if (istype(expr, SYMBOL)) {
    res = envlookup(expr, env);
  } else if (istype(expr, CONS)) {
    cell fn = nil;
    cell args = nil;
    GCPROTECT(&expr, &env, &fn, &args);
    gc();

    if (car(expr) == internc("quote")) {
      res = cadr(expr);
    } else if (car(expr) == internc("fn")) {
      res = closure(cadr(expr), cdr(cdr(expr)), env);
    } else if (car(expr) == internc("cond")) {
      res = evalcond(cdr(expr), env);
    } else if (car(expr) == internc("progn")) {
      res = progn(cdr(expr), env);
    } else {
      fn = eval(car(expr), env);
      args = evallist(cdr(expr), env);
      res = apply(fn, args);
    }
    ENDGCPROTECT();
  } else {
    res = expr; // everything else is self-evaluating
  }

  return res;
}

// CALLERS MUST GCPROTECT THEIR LOCALS
cell apply(cell proc, cell args) {
  if ((proc & TAGMASK) == HEAPPTR) {
    cell* ptr = (cell*)asptr(proc);
    if ((*ptr & HHMASK) == HHPRIM) {
      struct primitive* primitive = (struct primitive*)ptr;
      return primitive->fn(args);
    } else if ((*ptr & HHMASK) == HHCLOS) {
      struct closure* closure = (struct closure*)ptr;
      cell envframe = pairlis(closure->argnames, args);
      return progn(closure->body, cons(envframe, closure->env));
    }
  }
  printf("ERR attempt to call non-procedure: ");
  println(proc);
  printf("\n");
  return nil;
}


// READER/WRITER

char* text; // remaining input
#define OK 0
#define ERRPARSE 1
#define ERREOF 2
int err;

int unexpectedeof(char const *msg) {
  if (!*text) {
    printf("ERR eof %s\n", msg);
    err = ERRPARSE;
    return 1;
  }
  return 0;
}

void skipws(void) {
  for (;;) {
    if (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
      text++;
    else if (*text == ';')
      while (*text && *text != '\n') text++;
    else break;
  }
}

cell readform(void);
cell readlist(void) {
  skipws();
  if (unexpectedeof("while parsing list")) return nil;
  if (*text == ')') { text++; return nil; }
  if (*text == '.') {
    text++;
    cell cdr = readform();
    if (err) return nil;
    // consume end ')'
    skipws();
    if (unexpectedeof("after cons '.'")) return nil;
    if (*text != ')') {
      printf("parse error with (a . b) cons\n");
      err = 1;
      return nil;
    }
    text++;
    return cdr;
  }
  cell car = readform();
  if (err) return nil;
  cell cdr = readlist();
  if (err) return nil;
  return cons(car, cdr);
}

cell readform(void) {
  skipws();
  if (*text == '(') {
    text++;
    cell list = readlist();
    if (err) return nil;
    return list;
  }

  if (*text >= '0' && *text <= '9') {
    int n = 0;
    while (*text >= '0' && *text <= '9') {
      n = (n * 10) + (*text++ - '0');
    }
    return fix(n);
  }

  if (*text == '\'') {
    text++;
    cell inner = readform();
    if (err) return nil;
    return cons(internc("quote"), cons(inner, nil));
  }

  if (*text == '"') {
    char* start = ++text;
    while(*text && *text != '"') text++;
    cell strlit = str(start, text - start);
    if (unexpectedeof("in string literal")) return nil;
    text++;
    return strlit;
  }

  char* symstart = text;
  while(*text >= '!' && *text <= '~' && *text != '(' && *text != ')' && *text != '.')
    text++;
  if (text != symstart)
    return intern(symstart, text - symstart);

  if (!*text) {
    err = ERREOF; // Not neccessarily fatal
    return nil;
  }
  printf("ERR: unexpected char '%c' (%d)", *text, (int)(*text));
  err = 1;
  return nil;
}

void printlist(cell c) {
  printexpr(car(c));
  cell thecdr = cdr(c);
  if (thecdr == nil) {
    //end of proper list
  } else if (istype(thecdr, CONS)) {
    printf(" ");
    printlist(thecdr);
  } else {
    printf(" . ");
    printexpr(thecdr);
  }
}

void printexpr(cell c) {
  // print GC forwarded values
  if (isptrtype(c) && isinmainheap((void*)asptr(c)) && isforwarded((void*)asptr(c))){
    printf("fwd:");
    c = (c & TAGMASK) | (*((cell*)asptr(c))); // replace pointer part
  }

  if (isfix(c)) {
    printf("%ld", getfix(c));
  } else if (istype(c, CONS)) {
    printf("(");
    printlist(c);
    printf(")");
  } else if (istype(c, SYMBOL)) {
    printf("%s", getstr(c).data);
  } else if (istype(c, HEAPPTR)) {
    cell* ptr = (cell*)asptr(c);
    struct closure* closure;
    struct buffer* buf;
    switch(*ptr & HHMASK) {
      case HHCLOS:
        closure = (struct closure*)ptr;
        printf("#<clos ");
        printexpr(closure->argnames);
        printf(" ");
        printexpr(closure->body);
        printf(" ");
        printexpr(closure->env);
        printf(">");
        break;
      case HHPRIM:
        printf("#<prim %lx>", asptr(ptr[1]));
        break;
      case HHBUF:
        buf = (struct buffer*)ptr;
        printf("#<buf");
        for(char* c = buf->data; c < (buf->data + (buf->hh >> 8)); c++)
          printf(" %02x", (int)*c);
        printf(">");
        break;
      case HHSTR:
        printf("\"%s\"", getstr(c).data);
        break;
      default: printf("?"); break;
    }
  }
}

void println(cell c) {
  printexpr(c);
  printf("\n");
}


// PRIMITIVES

cell pplus(cell args) {
  if (args == nil) return fix(0);
  assert(isfix(car(args)));
  return fix(getfix(car(args)) + getfix(pplus(cdr(args))));
}
cell ptimes(cell args) {
  if (args == nil) return fix(1);
  assert(isfix(car(args)));
  return fix(getfix(car(args)) * getfix(ptimes(cdr(args))));
}
cell pminus(cell args) {
  return fix(getfix(car(args)) - getfix(cadr(args)));
}
cell pcons(cell args) { return cons(car(args), cadr(args)); }
cell pcar(cell args) { return car(car(args)); }
cell pcdr(cell args) { return cdr(car(args)); }
cell psetcar(cell args) {
  assert(istype(car(args), CONS));
  ((struct cons*)asptr(car(args)))->car = cadr(args);
  return car(args);
}
cell psetcdr(cell args) {
  assert(istype(car(args), CONS));
  ((struct cons*)asptr(car(args)))->cdr = cadr(args);
  return car(args);
}
cell pprint(cell args) {
  println(car(args));
  return nil;
}
cell passoc(cell args) { return assoc(car(args), cadr(args)); }
cell pdef(cell args) {
  globalenv = cons(cons(car(args), cadr(args)), globalenv);
  return nil;
}
cell peq(cell args) { return (car(args) == cadr(args)) ? fix(1) : nil; }
cell ppairlis(cell args) { return pairlis(car(args), cadr(args)); }
cell pgetbuf(cell args) {
  assert(isptrtype(car(args)) && isfix(cadr(args)));
  struct bufinner b = getstr(car(args));
  int index = getfix(cadr(args));
  assert(index < b.len);
  return fix(getstr(car(args)).data[index]);
}
cell psetbuf(cell args) {
  assert(isptrtype(car(args)) && isfix(cadr(args)));
  struct bufinner b = getstr(car(args));
  int index = getfix(cadr(args));
  assert(index < b.len);
  int val = getfix(cadr(cdr(args)));
  getstr(car(args)).data[index] = val;
  return fix(val);
}
cell pbuflen(cell args) {
  assert(isptrtype(car(args)));
  return fix(((struct buffer*)asptr(car(args)))->hh >> 8);
}
cell preadfile(cell args) {
  char* filename = getstr(car(args)).data;
  FILE* f = fopen(filename, "r");
  if (!f) { printf("cannot open %s\n", filename); return nil; }
  long filesize;
  fseek(f, 0, SEEK_END); filesize = ftell(f); rewind(f);
  cell bufcell = buffer(filesize+1);
  struct buffer* buf = (struct buffer*)asptr(bufcell);
  fread(buf->data, 1, filesize, f);
  fclose(f);
  buf->data[filesize] = '\0';
  return bufcell;
}
cell pwritefile(cell args) {
  char* filename = getstr(car(args)).data;
  struct buffer* buf = (struct buffer*)asptr(cadr(args));
  FILE* f = fopen(filename, "w");
  if (!f) { printf("cannot open %s\n", filename); return nil; }
  int written = fwrite(buf->data, 1, buf->hh >> 8, f);
  fclose(f);
  if (written != buf->hh >> 8) {
    printf("failed to write to %s\n", filename);
    return nil;
  }
  return fix(1);
}

void defprimitive(char* name, primitivefn fn) {
  globalenv = cons(cons(internc(name), prim(fn)), globalenv);
}


// IO

void repl(int bellsandwhistles) {
  for (;;) {
    if (bellsandwhistles) printf("> ");
    fflush(stdout);
    char line[0x1000];
    if (!fgets(line, sizeof(line), stdin)) break;
    text = line;
    err = OK;
    while (err == OK) {
      cell form = readform();
      if (err != OK)
        break;
      cell res  = eval(form, nil);
      println(res);
      gc();
      if (bellsandwhistles) printf("heap: %ld/%d\n", heaptop - heap, HEAPSIZE);
    }
  }
}

int main(int argc, char** argv) {
  heapa = heap = heaptop = malloc(HEAPSIZE);
  heapb = toheap = toheaptop = malloc(HEAPSIZE);
  nil = symc("nil");
  internlist = cons(nil, nil);
  globalenv = nil;
  GCPROTECT(&nil, &internlist, &globalenv);

  defprimitive("+", pplus);
  defprimitive("-", pminus);
  defprimitive("*", ptimes);
  defprimitive("car", pcar);
  defprimitive("cdr", pcdr);
  defprimitive("cons", pcons);
  defprimitive("print", pprint);
  defprimitive("setcar", psetcar);
  defprimitive("setcdr", psetcdr);
  defprimitive("assoc", passoc);
  defprimitive("def", pdef);
  defprimitive("eq", peq);
  defprimitive("pairlis", ppairlis);
  defprimitive("getbuf", pgetbuf);
  defprimitive("setbuf", psetbuf);
  defprimitive("buflen", pbuflen);
  defprimitive("readfile", preadfile);
  defprimitive("writefile", pwritefile);

  if (argc <= 1) {
    printf("Nothing to do. `%s repl` for repl\n", argv[0]);
    return 0;
  }
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "repl")) {
      repl(1);
      return 0;
    } else if (!strcmp(argv[i], "stdin")) {
      repl(0);
      return 0;
    }

    FILE* f = fopen(argv[i], "r");
    if (!f) { printf("cannot open %s\n", argv[i]); return 1; }
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    char* src = malloc(filesize+1);
    fread(src, 1, filesize, f);
    fclose(f);
    src[filesize] = '\0';
    text = src;
    err = 0;
    while(*text) {
      skipws();
      if (!*text) break;
      cell form = readform();
      if (err) { printf("ERR parsing %s\n", argv[i]); return 1; }
      eval(form, nil);
    }
    free(src);
  }
}

// vim: set ts=2 sw=2 :
