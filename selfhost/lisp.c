#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define HEAPALIGN 0x8 // 3byte tag = 8 byte align

// TAG for fix is single low bit of zero
#define FIXTAG  0x1 // mask
#define FIX     0x0 // fixed int

#define TAG     0x7 // (mask)
#define TCONS   0x1 // cons
#define TSYM    0x3 // symbol
#define TSTR    0x5 // string
#define TSTAG   0x7 // self tagged heap pointer

#define STAG    0xff // (mask)
#define STCLOS  0x00 // lisp closure
#define STPRIM  0x01 // primitive function

typedef long cell;
typedef cell (*primitivefn) (cell args);

struct gcframe {
  struct gcframe* parent;
  int cnt;
  cell** localptrs;
};

#define HEAPSIZE 0x10000
cell* heap;
cell* heaptop;
cell internlist;
cell nil;
cell globalenv;
struct gcframe* topgcframe = NULL;

#define GCPROTECT(_n, ...) \
  cell* _localptrs[_n] = { __VA_ARGS__ }; \
  struct gcframe _frame = { topgcframe, _n, _localptrs }; \
  topgcframe = &_frame;

#define ENDGCPROTECT() topgcframe = _frame.parent;


void println(cell c);
void printexpr(cell c);

void* alloc(int cells) {
  cell* p = heaptop;
  heaptop += cells;
  heaptop = (cell*)(((cell)heaptop + HEAPALIGN - 1) & ~(HEAPALIGN - 1));
  return p;
}

char* allocbytes(int bytes) {
  return alloc((bytes + sizeof(cell) - 1) / sizeof(cell));
}

void gc(struct gcframe* frame) {
  return;
  if (!frame)
    return;
  if (frame == topgcframe)
    printf("====================\nGC invoked\n");
  printf("frame:\n");
  for (int i = 0; i < frame->cnt; i++) {
    printf("   ");
    printexpr(*frame->localptrs[i]);
    printf("\n");
  }
  gc(frame->parent);
}

struct cons { cell car, cdr; };
struct primitive { cell stag; primitivefn fn; };
struct closure { cell stag, argnames, body, env; };

#define istype(_cell, _type) ((_cell & TAG) == _type)

cell cons(cell car, cell cdr) {
  struct cons* cns = alloc(sizeof(struct cons) / sizeof(cell));
  cns->car = car;
  cns->cdr = cdr;
  return ((cell)cns) | TCONS;
}

cell car(cell cons) {
  if (cons == nil) return nil;
  assert(istype(cons, TCONS));
  return ((struct cons*)(cons & ~(TAG)))->car;
}

cell cdr(cell cons) {
  if (cons == nil) return nil;
  assert(istype(cons, TCONS));
  return ((struct cons*)(cons & ~(TAG)))->cdr;
}

#define cadr(_cons) car(cdr(_cons))
#define fix(n) ((cell)((n) << 1))

#define strc(s) str(s, strlen(s))
cell str(char* s, int len) {
  char* a = allocbytes(len);
  memcpy(a, s, len);
  return (cell)(a) | TSTR;
}

#define symc(s) sym(s, strlen(s))
cell sym(char* s, int len) { return (str(s, len) & ~TAG) | TSYM; }

char *getstr(cell strcell) {
  assert(istype(strcell, TSTR) || istype(strcell, TSYM));
  return (char*)(strcell & ~TAG);
}

#define internc(s) intern(s, strlen(s))
cell intern(char* s, int len) {
  cell rest = internlist;
  while (rest != nil) {
    char* name = getstr(car(rest));
    if ((len == strlen(name)) && !memcmp(name, s, len))
      return car(rest);
    rest = cdr(rest);
  }
  // make new
  internlist = cons(sym(s, len), internlist);
  return car(internlist);
}

cell prim(primitivefn fn) {
  struct primitive* primitive = alloc(sizeof(struct primitive) / sizeof(cell));
  primitive->stag = STPRIM;
  primitive->fn = fn;
  return (cell)primitive | TSTAG;
}

cell closure(cell argnames, cell body, cell env) {
  struct closure* closure = alloc(sizeof(struct closure) / sizeof(cell));
  closure->stag = STCLOS;
  closure->argnames = argnames;
  closure->body = body;
  closure->env = env;
  return (cell)closure | TSTAG;
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

  if (env == nil) {
    // Ideally we would treat global as just another env. But that breaks
    // recursive functions because the global environment they capture does not
    // yet contain themselves
    cell globalpair = assoc(sym, globalenv);
    if (globalpair != nil)
      return cdr(globalpair);
    printf("ERR unbound var: %s\n", getstr(sym));
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


cell evallist(cell list, cell env) {
  cell head = nil;
  cell res = nil;
  GCPROTECT(3, &list, &env, &head);

  if (list != nil) {
    head = eval(car(list), env);
    cell tail = evallist(cdr(list), env);
    res = cons(head, tail);
  }

  ENDGCPROTECT();
  return res;
}

cell evalcond(cell branches, cell env) {
  if (branches == nil) return nil;
  if (eval(car(car(branches)), env) != nil)
    return eval(cadr(car(branches)), env);
  return evalcond(cdr(branches), env);
}

cell apply(cell proc, cell args);

cell progn(cell bodylist, cell env) {
  if (cdr(bodylist) == nil) {
    // base case requires no gc protect due to tail call
    return eval(car(bodylist), env);
  }

  GCPROTECT(2, &bodylist, &env);
  (void)eval(car(bodylist), env);
  ENDGCPROTECT();
  return progn(cdr(bodylist), env);
}


cell eval(cell expr, cell env) {
  GCPROTECT(2, &expr, &env);
  gc(topgcframe);

  cell res = nil;

  if ((expr & FIXTAG) == FIX) {
    res = expr;
  } else if ((expr & TAG) == TSTR) {
    res = expr;
  } else if ((expr & TAG) == TSYM) {
    res = envlookup(expr, env);
  } else if ((expr & TAG) == TCONS) {
    cell op = car(expr);
    if (op == internc("quote")) {
      res = cadr(expr);
    } else if (op == internc("fn")) {
      res = closure(cadr(expr), cdr(cdr(expr)), env);
    } else if (op == internc("cond")) {
      res = evalcond(cdr(expr), env);
    } else if (op == internc("progn")) {
      res = progn(cdr(expr), env);
    } else {
      // needs to be in separate steps so that fn doesn't get invalidated by
      // potential GC while evaluating args
      cell fn = eval(car(expr), env);
      cell args = evallist(cdr(expr), env);
      res = apply(fn, args);
    }
  }

  ENDGCPROTECT();
  return res;
}

cell apply(cell proc, cell args) {
  if ((proc & TAG) == TSTAG) {
    cell* heapval = (cell*)(proc & ~TAG);
    if ((heapval[0] & STAG) == STPRIM) {
      struct primitive* primitive = (struct primitive*)heapval;
      return primitive->fn(args);
    } else if ((heapval[0] & STAG) == STCLOS) {
      struct closure* closure = (struct closure*)heapval;
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

char* text;
int err;

int eof(char const *msg) {
  if (!*text) {
    printf("ERR eof %s\n", msg);
    err = 1;
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

cell readexpr(void);
cell readlist(void) {
  skipws();
  if (eof("while parsing list")) return nil;
  if (*text == ')') { text++; return nil; }
  if (*text == '.') {
    text++;
    cell cdr = readexpr();
    if (err) return nil;
    // consume end ')'
    skipws();
    if (eof("after cons '.'")) return nil;
    if (*text != ')') {
      printf("parse error with (a . b) cons\n");
      err = 1;
      return nil;
    }
    text++;
    return cdr;
  }
  cell car = readexpr();
  if (err) return nil;
  cell cdr = readlist();
  if (err) return nil;
  return cons(car, cdr);
}

cell readexpr(void) {
  skipws();
  if (*text == '(') {
    text++;
    cell list = readlist();
    if (err) return nil;
    return list;
  }

  if (*text >= '0' && *text <= '9') {
    int fix = 0;
    while (*text >= '0' && *text <= '9') {
      fix = (fix * 10) + (*text++ - '0');
    }
    return (cell)(fix << 1);
  }

  if (*text == '\'') {
    text++;
    cell inner = readexpr();
    if (err) return nil;
    return cons(internc("quote"), cons(inner, nil));
  }

  if (*text == '"') {
    char* start = ++text;
    while(*text && *text != '"') text++;
    cell strlit = str(start, text - start);
    if (eof("in string literal")) return nil;
    text++;
    return strlit;
  }

  char* symstart = text;
  while(*text >= '!' && *text <= '~' && *text != '(' && *text != ')' && *text != '.')
    text++;
  if (text != symstart)
    return intern(symstart, text - symstart);

  if (eof("while parsing expr")) return nil;
  printf("ERR: unexpected char '%c' (%d)", *text, (int)(*text));
  err = 1;
  return nil;
}

cell readstring(char* s) {
  text = s;
  err = 0;
  cell expr = readexpr();
  return err ? nil : expr;
}

void printlist(cell c) {
  printexpr(car(c));
  cell thecdr = cdr(c);
  if (thecdr == nil) {
    //end of proper list
  } else if ((thecdr & TAG) == TCONS) {
    printf(" ");
    printlist(thecdr);
  } else {
    printf(" . ");
    printexpr(thecdr);
  }
}

void printexpr(cell c) {
  if ((c & FIXTAG) == FIX) {
    printf("%ld", c >> 1);
  } else if ((c & TAG) == TCONS) {
    printf("(");
    printlist(c);
    printf(")");
  } else if ((c & TAG) == TSTR) {
    printf("\"%s\"", getstr(c));
  } else if ((c & TAG) == TSYM) {
    printf("%s", getstr(c));
  } else if ((c & TAG) == TSTAG) {
    cell* heapval = (cell*)(c & ~TAG);
    struct closure* closure;
    switch(heapval[0] & STAG) {
      case STPRIM:
        printf("#<prim>");
        break;
      case STCLOS:
         closure = (struct closure*)heapval;
         printf("#<clos ");
         printexpr(closure->argnames);
         printf(">");
        break;
    }
  }
}

void println(cell c) {
  printexpr(c);
  printf("\n");
}

// PRIMITIVES

cell pplus(cell args) {
  if (args == nil) return 0;
  else return fix((car(args)>>1) + (pplus(cdr(args))>>1));
}
cell ptimes(cell args) {
  if (args == nil) return 0;
  else return fix((car(args)>>1) * (pplus(cdr(args))>>1));
}
cell pminus(cell args) { return fix((car(args)>>1) - (cadr(args)>>1)); }
cell pcons(cell args) { return cons(car(args), cadr(args)); }
cell pcar(cell args) { return car(car(args)); }
cell pcdr(cell args) { return cdr(car(args)); }
cell psetcar(cell args) {
  assert(istype(car(args), TCONS));
  ((struct cons*)(car(args) & ~TAG))->car = cadr(args);
  return car(args);
}
cell psetcdr(cell args) {
  assert(istype(car(args), TCONS));
  ((struct cons*)(car(args) & ~TAG))->cdr = cadr(args);
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

void defprimitive(char* name, primitivefn fn) {
  globalenv = cons(cons(internc(name), prim(fn)), globalenv);
}

void repl(void) {
  for (;;) {
    printf("> ");
    fflush(stdout);
    char line[0x1000];
    if (!fgets(line, sizeof(line), stdin)) break;
    cell form = readstring(line);
    if (err) continue;
    println(eval(form, nil));
    printf("heap: %ld/%d\n", heaptop - heap, HEAPSIZE);
  }
}

int main(int argc, char** argv) {
  heap = heaptop = malloc(HEAPSIZE);
  nil = symc("nil");
  internlist = cons(nil, nil);
  globalenv = nil;
  GCPROTECT(3, &nil, &internlist, &globalenv);

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

  if (argc <= 1) {
    printf("Nothing to do. `%s repl` for repl\n", argv[0]);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "repl")) {
      repl();
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
      cell form = readexpr();
      if (err) { printf("ERR parsing %s\n", argv[i]); return 1; }
      eval(form, nil);
    }
    free(src);
  }
}

// vim: set ts=2 sw=2 :
