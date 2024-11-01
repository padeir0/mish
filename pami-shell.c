#include "pami-shell.h"

/* BEGIN: ARENA ALLOCATOR */
enum arena_RES {
  arena_OK,
  arena_NULL_BUFF,
  arena_TOO_SMALL
};

char* arena_str_res(enum arena_RES res);

typedef struct {
  uint8_t* buffer;
  size_t   buffsize;
  size_t   allocated;
} arena;

/* returns a arena allocated at the beginning of the buffer */
arena* arena_create(uint8_t* buffer, size_t size, enum arena_RES* res);

/* returns NULL if it fails to allocate */
void* arena_alloc(arena* a, size_t size);

/* frees the entire arena */
void arena_free_all(arena* a);

/* returns the amount of memory available */
size_t arena_available(arena* a);

/* returns the amount of memory used */
size_t arena_used(arena* a);

/* returns true if the arena is empty */
bool arena_empty(arena* a);

/* returns the head of the arena */
void* arena_head(arena* a);

char* arena_str_res(enum arena_RES res) {
  switch(res){
    case arena_OK:
      return "OK";
    case arena_NULL_BUFF:
      return "Provided buffer is null";
    case arena_TOO_SMALL:
      return "Provided buffer is too small";
  }
  return "???";
}

size_t distance(uint8_t* a, uint8_t* b) {
  if (a > b) {
    return a-b;
  } else {
    return b-a;
  }
}

arena* arena_create(uint8_t* buffer, size_t size, enum arena_RES* res) {
  arena* out;
  if (buffer == NULL) {
    *res = arena_NULL_BUFF;
    return NULL;
  }

  if (size < sizeof(arena)) {
    *res = arena_TOO_SMALL;
    return NULL;
  }

  out = (arena*)buffer;
  out->buffer = buffer + sizeof(arena);
  out->buffsize = size - sizeof(arena);
  out->allocated = 0;

  return out;
}

void* arena_head(arena* a) {
  return (void*)(a->buffer + a->allocated);
}

void* arena_alloc(arena* a, size_t size) {
  void* out = arena_head(a);
  if (a->allocated+size >= a->buffsize) {
    return NULL;
  }
  a->allocated += size;
  return out;
}

void arena_free_all(arena* a) {
  a->allocated = 0;
}

size_t arena_available(arena* a) {
  return a->buffsize - a->allocated;
}

size_t arena_used(arena* a) {
  return a->allocated;
}

bool arena_empty(arena* a) {
  return a->allocated == 0;
}
/* END: ARENA ALLOCATOR*/

/* BEGIN: UTF-8 DECODER */
#define LOW_BITS(n) (uint8_t)((1<<n)-1)
#define TOP_BITS(n) (uint8_t)(~((1<<(8-n))-1))

typedef int32_t rune;

#define EoF (rune)0

size_t utf8_decode(const char* buffer, rune* r) {
  if ((buffer[0] & TOP_BITS(1)) == 0) { /* ASCII */
    *r = (rune)buffer[0];
    return 1;
  }

  /* TWO BYTE SEQUENCE */
  if ((buffer[0] & TOP_BITS(3)) == TOP_BITS(2)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (rune)(buffer[0] & LOW_BITS(5)) << 6 |
         (rune)(buffer[1] & LOW_BITS(6));
    return 2;
  }

  /* THREE BYTE SEQUENCE */
  if ((buffer[0] & TOP_BITS(4)) == TOP_BITS(3)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[2] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (rune)(buffer[0] & LOW_BITS(4)) << 12 |
         (rune)(buffer[1] & LOW_BITS(6)) << 6 |
         (rune)(buffer[2] & LOW_BITS(6));
    return 3;
  }

  /* FOUR BYTE SEQUENCE */
  if ((buffer[0] & TOP_BITS(5)) == TOP_BITS(4)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[2] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[3] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (rune)(buffer[0] & LOW_BITS(3)) << 18 |
         (rune)(buffer[1] & LOW_BITS(6)) << 12 |
         (rune)(buffer[2] & LOW_BITS(6)) << 6 |
         (rune)(buffer[3] & LOW_BITS(6));
    return 4;
  }

  /* INVALID ENCODING */
  *r = -1;
  return 0;
}
/* END: UTF-8 DECODER */


/* BEGIN: LEXER */
enum lex_kind {
  lk_bad,
  lk_num,
  lk_str,
  lk_colon,
  lk_id,
  lk_newline,
  lk_eof
};

enum val_kind {
  vk_none,
  vk_exact_num,
  vk_inexact_num,
  vk_boolean
};

typedef union {
  uint64_t exact_num;
  double   inexact_num;
  bool     boolean;
} lex_value;

typedef struct {
  enum lex_kind kind;
  enum val_kind vkind;

  size_t begin;
  size_t end;

  lex_value value;
} lexeme;

typedef struct {
  const char* input;
  size_t input_size;
  lexeme lexeme;
  error err;
} lexer;

lexer lex_new_lexer(const char* input, size_t size) {
  lexer l;
  l.input = input;
  l.input_size = size;
  l.lexeme.begin = 0;
  l.lexeme.end = 0;
  l.lexeme.vkind = vk_none;
  l.lexeme.kind = lk_bad;
  return l;
}

error lex_base_err(lexer* l) {
  error err;
  err.code = error_none;
  err.range.begin = l->lexeme.begin;
  err.range.end = l->lexeme.end;
  return err;
}

error lex_err_bad_rune(lexer* l) {
  error err = lex_base_err(l);
  err.code = error_bad_rune;
  return err;
}

error lex_err_internal(lexer* l) {
  error err = lex_base_err(l);
  err.code = error_internal_lexer;
  return err;
}

error lex_err_unrecognized(lexer* l) {
  error err = lex_base_err(l);
  err.code = error_unrecognized_rune;
  return err;
}

rune lex_next_rune(lexer* l) {
  rune r;
  size_t size;

  if (l->lexeme.end >= l->input_size) {
    return EoF;
  }
  
  size = utf8_decode(l->input + l->lexeme.end, &r);
  if (size == 0 || r == -1) {
    l->err = lex_err_bad_rune(l);
    return -1;
  }
  l->lexeme.end += size;
  return r;
}

rune lex_peek_rune(lexer* l) {
  rune r;
  size_t size;
  if (l->lexeme.end >= l->input_size) {
    return EoF;
  }

  size = utf8_decode(l->input + l->lexeme.end, &r);
  
  if (size == 0 || r == -1) {
    l->err = lex_err_bad_rune(l);
    return -1;
  }
  return r;
}

void lex_ignore(lexer* l) {
  l->lexeme.begin = l->lexeme.end;
  l->lexeme.kind = lk_bad;
}

typedef bool (*validator)(rune);

bool lex_is_decdigit(rune r) {
  return (r >= '0' && r <= '9') ||
         (r == '_');
}

bool lex_is_hexdigit(rune r) {
  return (r >= '0' && r <= '9') ||
         (r >= 'a' && r <= 'f') ||
         (r >= 'A' && r <= 'F') ||
         (r == '_');
}

bool lex_is_bindigit(rune r) {
  return (r == '0') ||
         (r == '1') ||
         (r == '_');
}

bool lex_is_idchar(rune r) {
  return (r >= 'a' && r <= 'z') ||
         (r >= 'A' && r <= 'Z') ||
         (r == '~') || (r == '+') ||
         (r == '-') || (r == '_') ||
         (r == '*') || (r == '/') ||
         (r == '?') || (r == '=') ||
         (r == '&') || (r == '$') ||
         (r == '%') || (r == '<') ||
         (r == '>') || (r == '!');
}

bool lex_is_idcharnum(rune r) {
  return (r >= '0' && r <= '9') ||
         (r >= 'a' && r <= 'z') ||
         (r >= 'A' && r <= 'Z') ||
         (r == '~') || (r == '+') ||
         (r == '-') || (r == '_') ||
         (r == '*') || (r == '/') ||
         (r == '?') || (r == '=') ||
         (r == '&') || (r == '$') ||
         (r == '%') || (r == '<') ||
         (r == '>') || (r == '!');
}

bool lex_is_whitespace(rune r) {
  return (r == ' ') || (r == '\r') || (r == '\t');
}

bool lex_is_special_dq_str_char(rune r) {
  return (r == '\\') || (r == '"');
}

bool lex_is_special_sq_str_char(rune r) {
  return (r == '\\') || (r == '\''');
}

bool lex_accept_run(lexer* l, validator v) {
  rune r = lex_peek_rune(l);
  if (r < 0) {
    return false;
  }
  while (v(r)) {
    lex_next_rune(l);
    r = lex_peek_rune(l);
    if (r < 0) {
      return false;
    }
  }

  return true;
}

bool lex_accept_until(lexer* l, validator v) {
  rune r = lex_peek_rune(l);
  if (r < 0) {
    return false;
  }
  while (!v(r)) {
    lex_next_rune(l);
    r = lex_peek_rune(l);
    if (r < 0) {
      return false;
    }
  }

  return true;
}

bool lex_read_strlit(lexer* l, char delim) {
  rune r = lex_peek_rune(l);
  bool ok;
  validator val;
  if (r != delim) {
    l->err = lex_err_internal(l);
    return false;
  }
  lex_next_rune(l);

  val = lex_is_special_dq_str_char;
  if (delim == '\'') {
    val = lex_is_special_sq_str_char;
  }

  while (true) {
    ok = lex_accept_until(l, val);
    if (ok == false) {
      return false;
    }

    if (r == delim) {
      lex_next_rune(l);
      return true;
    }

    if (r == '\\') {
      lex_next_rune(l);
      ok = lex_next_rune(l);
      if (ok == false) {
        return false;
      }
    }
  }
}

bool lex_conv_hex(lexer* l, uint64_t* value) {
  /* jump the '0x' */
  char* begin = (char*)l->input + l->lexeme.begin + 2;
  char* end = (char*)l->input + l->lexeme.end;
  char c;
  uint64_t output = 0;
  while (begin < end) {
    c = *begin;
    if (c == '_') {
      begin++;
      continue;
    }
    output *= 16;
    if (c >= '0' && c <= '9') {
      output += (c - '0');
    } else if (c >= 'a' && c <= 'z') {
      output += (c - 'a') + 10;
    } else if (c >= 'A' && c <= 'Z') {
      output += (c - 'A') + 10;
    } else {
      l->err = lex_err_internal(l);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_bin(lexer* l, uint64_t* value) {
  /* jump the '0b' */
  char* begin = (char*)l->input + l->lexeme.begin + 2;
  char* end = (char*)l->input + l->lexeme.end;
  char c;
  uint64_t output = 0;
  while (begin < end) {
    c = *begin;
    if (c == '_') {
      begin++;
      continue;
    }
    output *= 2;
    if (c == '0' || c == '1') {
      output += (c - '0');
    } else {
      l->err = lex_err_internal(l);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_dec(lexer* l, uint64_t* value) {
  char* begin = (char*)l->input + l->lexeme.begin;
  char* end = (char*)l->input + l->lexeme.end;
  char c;
  uint64_t output = 0;
  while (begin < end) {
    c = *begin;
    if (c == '_') {
      begin++;
      continue;
    }
    output *= 10;
    if (c >= '0' && c <= '9') {
      output += (c - '0');
    } else {
      l->err = lex_err_internal(l);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_inexact(lexer* l, double* value) {
  char* begin = (char*)l->input + l->lexeme.begin;
  char* end = (char*)l->input + l->lexeme.end;
  char c;
  int fractional = 0;
  double output = 0;
  while (begin < end) {
    c = *begin;
    if (c == '_') {
      begin++;
      continue;
    }
    if (c == '.') {
      fractional += 1;
      begin++;
      continue;
    }

    if (c >= '0' && c <= '9' && fractional == 0) {
      output *= 10;
      output += (c - '0');
    } else if (c >= '0' && c <= '9' && fractional > 0) {
      output += (double)(c - '0') / (10.0*fractional);
      fractional += 1;
    } else {
      l->err = lex_err_internal(l);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_read_number(lexer* l) {
  rune r = lex_peek_rune(l);
  bool ok;
  uint64_t exact_value;
  double inexact_value;
  if (r < 0) {
    return false;
  }
  if (r == '0') {
    lex_next_rune(l);
    r = lex_peek_rune(l);
    switch (r) {
      case 'x':
        lex_next_rune(l);
        ok = lex_accept_run(l, lex_is_hexdigit);
        if (ok == false) {
          return false;
        }
        ok = lex_conv_hex(l, &exact_value);
        if (ok == false) {
          return false;
        }
        l->lexeme.value.exact_num = exact_value;
        l->lexeme.vkind = vk_exact_num;
        l->lexeme.kind = lk_num;
        return true;
      case 'b':
        lex_next_rune(l);
        ok = lex_accept_run(l, lex_is_bindigit);
        if (ok == false) {
          return false;
        }
        ok = lex_conv_bin(l, &exact_value);
        if (ok == false) {
          return false;
        }
        l->lexeme.value.exact_num = exact_value;
        l->lexeme.vkind = vk_exact_num;
        l->lexeme.kind = lk_num;
        return true;
    }
  }
  ok = lex_accept_run(l, lex_is_decdigit);
  if (ok == false) {
    return false;
  }
  r = lex_peek_rune(l);
  if (r == '.') {
    lex_next_rune(l);
    ok = lex_accept_run(l, lex_is_decdigit);
    if (ok == false) {
      return false;
    }
    ok = lex_conv_inexact(l, &inexact_value);
    if (ok == false) {
      return false;
    }
    l->lexeme.value.inexact_num = inexact_value;
    l->lexeme.vkind = vk_inexact_num;
    l->lexeme.kind = lk_num;
  } else {
    ok = lex_conv_dec(l, &exact_value);
    if (ok == false) {
      return false;
    }
    l->lexeme.value.exact_num = exact_value;
    l->lexeme.vkind = vk_exact_num;
    l->lexeme.kind = lk_num;
  }
  return true;
}

bool lex_read_identifier(lexer* l) {
  rune r = lex_peek_rune(l);
  bool ok; bool value;
  if (lex_is_idchar(r) == false){
    l->err = lex_err_internal(l);
    return false;
  }
  l->lexeme.kind = lk_id;
  ok = lex_accept_run(l, lex_is_idcharnum);
  if (ok == false) {
    return false;
  }
  return true;
}

bool lex_ignore_whitespace(lexer* l) {
  rune r = lex_peek_rune(l);
  bool ok;
  if (r < 0) {
    return false;
  }
  while (true) {
    if (lex_is_whitespace(r)) {
      lex_next_rune(l);
    } else if (r == '#') {
      ok = lex_read_comment(l);
      if (ok == false) {
        return false;
      }
    } else {
      break;
    }

    r = lex_peek_rune(l);
    if (r < 0) {
      return false;
    }
  }
  lex_ignore(l);
  return true;
}

bool lex_read_any(lexer* l) {
  rune r;
  bool ok = lex_ignore_whitespace(l);
  if (ok == false) {
    return false;
  }

  r = lex_peek_rune(l);
  if (lex_is_decdigit(r)) {
    return lex_read_number(l);
  }
  if (lex_is_idchar(r)){
    return lex_read_identifier(l);
  }
  switch (r) {
    case '"':
      return lex_read_strlit(l, '"');
    case '\'':
      return lex_read_strlit(l, '\'');
    case ':':
      lex_next_rune(l);
      l->lexeme.kind = lk_colon;
      break;
    case '\n':
      lex_next_rune(l);
      l->lexeme.kind = lk_newline;
      break;
    case EoF:
      l->lexeme.kind = lk_eof;
      break;
    default:
      l->err = lex_err_unrecognized(l);
      return false;
  }
  return true;
}

/*
 * it returns true if everything went smoothly,
 * and returns false if there was some error.
 * the error is stored in lexer.err
 */
bool lex_next(lexer* l) {
  l->lexeme.begin = l->lexeme.end;
  return lex_read_any(l);
}
/* END: LEXER */

/* BEGIN: PARSER*/
/* parser is technically recursive descent, but without
 * any recursion, since the grammar is not recursive
 */

error error_parser_out_of_memory(lexer* l) {
  error err = lex_base_err(l);
  err.code = error_parser_out_of_memory;
  return err;
}

arg_list* pr_parse(char* input, size_t input_size, arena* alloc, arena* str_alloc, error* err) {
  lexer* l = lex_new_lexer(input, input_size);
  arg_list* list;
  argument* arg;
  bool ok;

  ok = lex_next(l);
  if (!ok) {
    *err = l.err;
    return NULL;
  }

  list = (arg_list*) arena_alloc(alloc, sizeof(arg_list));
  if (list == NULL) {
    *err = error_parser_out_of_memory(l);
    return NULL;
  }
  list->argv = (argument*) arena_head(alloc);
  while (pr_parse_args(l, alloc, str_alloc, err));
  if (err.code != error_none) {
    return NULL;
  }
  return list;
}

bool pr_parse_args(lexer* l, arena* alloc, error* err) {
  atom at1;
  atom at2;
  pair p;
  argument* a;
  if (pr_parse_atom(l, &at1, err) == false) {
    return false;
  }
  if (ok == false) {
    return false;
  }
  a = arena_alloc(alloc, sizeof(argument));
  if (a == NULL) {
    *err = error_parser_out_of_memory(l);
    return false;
  }
  if (l->lexeme.kind == lk.colon) {
    if (pr_parse_atom(l, &at2, err) == false) {
      return false;
    }
    p.key = at1;
    p.value = at2;
    a->kind = ark_pair;
    a->contents.pair = p;
    return true;
  }
  a->kind = ark_atom;
  a->contents.atom = at1;
  return true;
}

bool pr_parse_atom(lexer* l, atom* a, error* err) {
  switch (l->kind) {
    case lk_str:
      break;
    case lk_id:
      break;
    case lk_num:
      break;
    case lk_newline:
      break;
    case lk_eof:
      break;
  }
}
/* END: PARSER*/
