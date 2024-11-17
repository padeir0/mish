#include "mish.h"
#include <limits.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>

/* All public symbols start with "mish",
 * private names will omit this. */

/* BEGIN: UTIL NAMESPACE */
size_t util_compute_padding(size_t allocated) {
  size_t misalignment;
  size_t padding;
  misalignment = allocated & (sizeof(void*) - 1);
  if (misalignment == 0) {
    return 0;
  }
  padding = sizeof(void*) - misalignment;
  return padding;
}

size_t util_align_trim_down(size_t size) {
  size_t misalignment;
  misalignment = size & (sizeof(void*) - 1);
  return size - misalignment;
}
/* END: UTIL NAMESPACE */

/* BEGIN: ATOM NAMESPACE */
mish_atom mish_atom_create_num_exact(uint64_t value) {
  mish_atom a;
  a.kind = mish_atk_exact_num;
  a.contents.exact_num = value;
  return a;
}

mish_atom mish_atom_create_num_inexact(double value) {
  mish_atom a;
  a.kind = mish_atk_inexact_num;
  a.contents.inexact_num = value;
  return a;
}

/* this function does not copy the string
 * ensure the lifetimes of whatever you're doing
 * are precise.
 */
mish_atom mish_atom_create_str(char* s) {
  mish_atom a;
  mish_str string;
  string.buffer = s;
  string.length = strlen(s);
  a.kind = mish_atk_string;
  a.contents.string = string;
  return a;
}

mish_atom mish_atom_create_cmd(mish_command cmd) {
  mish_atom a;
  a.kind = mish_atk_command;
  a.contents.cmd = cmd;
  return a;
}

bool mish_atom_equals(mish_atom a, mish_atom b) {
  mish_str a_s; mish_str b_s;
  if (a.kind != b.kind) {
    return false;
  }

  switch (a.kind) {
    case mish_atk_string:
      a_s = a.contents.string;
      b_s = b.contents.string;

      if (a_s.length != b_s.length) {
        return false;
      }
      return strncmp(a_s.buffer,
                     b_s.buffer,
                     a_s.length) == 0;
    case mish_atk_exact_num:
      return a.contents.exact_num == b.contents.exact_num;
    case mish_atk_inexact_num:
      return a.contents.inexact_num == b.contents.inexact_num;
    case mish_atk_command:
      return a.contents.cmd == b.contents.cmd;
    default:
      /* unreachable */
      return false;
  }
}

bool mish_atom_is_exact(mish_atom a) {
  return a.kind == mish_atk_exact_num;
}

bool mish_atom_is_inexact(mish_atom a) {
  return a.kind == mish_atk_inexact_num;
}

bool mish_atom_is_cmd(mish_atom a) {
  return a.kind == mish_atk_command;
}

bool mish_atom_is_str(mish_atom a) {
  return a.kind == mish_atk_string;
}
/* END: ATOM NAMESPACE */

/* BEGIN: SNPRINT NAMESPACE */
size_t mish_snprint_atom(char* buffer, size_t size, mish_atom a) {
  size_t offset = 0;
  if (buffer == NULL) {
    return 0;
  }
  switch (a.kind) {
    case mish_atk_string:
      offset = snprintf(buffer, size, "\"%.*s\"",
                        (int)a.contents.string.length,
                        a.contents.string.buffer);
      break;
    case mish_atk_exact_num:
      offset = snprintf(buffer, size, "%lld", (long long int)a.contents.exact_num);
      break;
    case mish_atk_inexact_num:
      offset = snprintf(buffer, size, "%f", a.contents.inexact_num);
      break;
    case mish_atk_command:
      offset = snprintf(buffer, size, "<%llu>", (unsigned long long int)a.contents.cmd);
      break;
    default:
      /* should be unreachable */
      offset = snprintf(buffer, size, "Unknown atom kind (%d)", (int)a.kind);
      break;
  }
  return offset;
}

size_t mish_snprint_arg(char* buffer, size_t size, mish_argument a) {
  size_t offset = 0;
  mish_pair p;
  switch (a.kind) {
  case mish_ark_pair:
    p = a.contents.pair;
    offset += snprintf(buffer+offset, size-offset, "(");
    offset += mish_snprint_atom(buffer+offset, size-offset, p.key);
    offset += snprintf(buffer+offset, size-offset, ", ");
    offset += mish_snprint_atom(buffer+offset, size-offset, p.value);
    offset += snprintf(buffer+offset, size-offset, ")");
    break;
  case mish_ark_atom:
    offset += mish_snprint_atom(buffer, size, a.contents.atom);
    break;
  }
  return offset;
}

size_t mish_snprint_arg_list(char* buffer, size_t size, mish_arg_list* list) {
  size_t offset = 0;
  mish_arg_list* curr;
  if (list == NULL) {
    return snprintf(buffer, size, "NULL");
  }

  curr = list;
  while (curr != NULL) {
    offset += mish_snprint_arg(buffer+offset, size-offset, curr->arg);

    if (curr->next != NULL) {
      offset += snprintf(buffer+offset, size-offset, ", ");
    }
    curr = curr->next;
  }
  return offset;
}
/* END: SNPRINT NAMESPACE */

/* BEGIN: ARENA NAMESPACE */
typedef enum {
  arena_OK,
  arena_NULL_BUFF,
  arena_TOO_SMALL
} arena_RES;

char* arena_str_res(arena_RES res);

/* returns a arena allocated at the beginning of the buffer */
mish_arena* arena_new(uint8_t* buffer, size_t size, arena_RES* res);

/* returns NULL if it fails to allocate */
void* arena_alloc(mish_arena* a, size_t size);

/* frees the entire arena */
void arena_free_all(mish_arena* a);

/* returns the amount of memory available */
size_t arena_available(mish_arena* a);

/* returns the amount of memory used */
size_t arena_used(mish_arena* a);

/* returns true if the arena is empty */
bool arena_empty(mish_arena* a);

/* returns the head of the arena */
void* arena_head(mish_arena* a);

mish_error_code arena_map_res(arena_RES res) {
  switch(res){
    case arena_OK:
      return mish_error_none;
    case arena_NULL_BUFF:
      return mish_error_arena_null_buffer;
    case arena_TOO_SMALL:
      return mish_error_arena_too_small;
  }
  return mish_error_internal;
}

char* arena_str_res(arena_RES res) {
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

mish_arena* arena_new(uint8_t* buffer, size_t size, arena_RES* res) {
  mish_arena* out;
  if (buffer == NULL) {
    *res = arena_NULL_BUFF;
    return NULL;
  }

  if (size < sizeof(mish_arena)) {
    *res = arena_TOO_SMALL;
    return NULL;
  }

  out = (mish_arena*)buffer;
  out->buffer = buffer + sizeof(mish_arena);
  out->buffsize = size - sizeof(mish_arena);
  out->allocated = 0;
  *res = arena_OK;

  return out;
}

void* arena_head(mish_arena* a) {
  if (a == NULL) return NULL;
  return (void*)(a->buffer + a->allocated);
}

void* arena_alloc(mish_arena* a, size_t size) {
  void* out;

  if (a == NULL || size == 0) return NULL;

  /* This assumes all architectures are aligned
   * in power-of-two chunks (ie, 2, 4, 8 bytes),
   * which is reasonable.
   */
  size += util_compute_padding(size);
  
  if (a->allocated+size >= a->buffsize) {
    return NULL;
  }

  out = arena_head(a);
  a->allocated += size;
  return out;
}

void arena_free_all(mish_arena* a) {
  if (a == NULL) return;
  a->allocated = 0;
  /* If you need to uncomment this line because of a bug,
   * you're doing something wrong.
   * However, if sensitive data is being transmitted through the
   * shell, this might be a good idea.
   */
  /* bzero(a->buffer, a->buffsize); */
}

size_t arena_available(mish_arena* a) {
  if (a == NULL) return 0;
  return a->buffsize - a->allocated;
}

size_t arena_used(mish_arena* a) {
  if (a == NULL) return 0;
  return a->allocated;
}

bool arena_empty(mish_arena* a) {
  if (a == NULL) return true;
  return a->allocated == 0;
}
/* END: ARENA ALLOCATOR*/

/* BEGIN: UTF8 NAMESPACE */
#define LOW_BITS(n) (uint8_t)((1<<n)-1)
#define TOP_BITS(n) (uint8_t)(~((1<<(8-n))-1))

typedef int32_t utf8_rune;

#define utf8_EoF (utf8_rune)0

/* Assumes valid UTF-8 input and does not handle:
 *   Overlong encodings
 *   Surrogate pairs
 *   Code points beyond U+10FFFF
 */
size_t utf8_decode(const char* buffer, size_t buff_size, utf8_rune* r) {
  if (buff_size > 0 && (buffer[0] & TOP_BITS(1)) == 0) { /* ASCII */
    *r = (utf8_rune)buffer[0];
    return 1;
  }

  /* TWO BYTE SEQUENCE */
  if (buff_size >= 2 && (buffer[0] & TOP_BITS(3)) == TOP_BITS(2)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (utf8_rune)(buffer[0] & LOW_BITS(5)) << 6 |
         (utf8_rune)(buffer[1] & LOW_BITS(6));
    return 2;
  }

  /* THREE BYTE SEQUENCE */
  if (buff_size >= 3 && (buffer[0] & TOP_BITS(4)) == TOP_BITS(3)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[2] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (utf8_rune)(buffer[0] & LOW_BITS(4)) << 12 |
         (utf8_rune)(buffer[1] & LOW_BITS(6)) << 6 |
         (utf8_rune)(buffer[2] & LOW_BITS(6));
    return 3;
  }

  /* FOUR BYTE SEQUENCE */
  if (buff_size >= 4 && (buffer[0] & TOP_BITS(5)) == TOP_BITS(4)) {
    if ((buffer[1] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[2] & TOP_BITS(2)) != TOP_BITS(1) ||
        (buffer[3] & TOP_BITS(2)) != TOP_BITS(1)) {
      *r = -1;
      return 0;
    }
    *r = (utf8_rune)(buffer[0] & LOW_BITS(3)) << 18 |
         (utf8_rune)(buffer[1] & LOW_BITS(6)) << 12 |
         (utf8_rune)(buffer[2] & LOW_BITS(6)) << 6 |
         (utf8_rune)(buffer[3] & LOW_BITS(6));
    return 4;
  }

  /* INVALID ENCODING */
  *r = -1;
  return 0;
}
/* END: UTF8 NAMESPACE */

/* BEGIN: MAP NAMESPACE */
/* implements a simple hashmap on top of a linked list
 * and a linear allocator. Because of the allocation strategy
 * used, "update" and "remove" procedures do not exist.
 * Just like the linear(arena) allocator, things can only
 * be removed from the map all at once.
 * I'd call this a "linear map" just to piss off mathematicians,
 * but it's better to call it a "linear hashmap".
 */
uint32_t map_murmur_hash(char* buff, size_t size) {
  uint32_t out = 0xCAFEBABE;
  size_t i;
  for (i = 0; i < size; i++) {
    out = out ^ (uint32_t)buff[i] * 0x5bd1e995;
    out = out ^ (out >> 15);
  }
  return out;
}

uint32_t map_hash_str(mish_str s) {
  return map_murmur_hash(s.buffer, s.length);
}

uint32_t map_hash_exact(uint64_t num) {
  return (uint32_t)(num % UINT_MAX);
}

uint32_t map_hash_inexact(double num) {
  return map_murmur_hash((char*)&num, sizeof(double));
}

uint32_t map_hash_cmd(mish_command cmd) {
  return (uint32_t)((uint64_t)cmd % UINT_MAX);
}

uint32_t map_hash(mish_atom a) {
  switch (a.kind) {
  case mish_atk_string:
    return map_hash_str(a.contents.string);
  case mish_atk_exact_num:
    return map_hash_exact(a.contents.exact_num);
  case mish_atk_inexact_num:
    return map_hash_inexact(a.contents.inexact_num);
  case mish_atk_command:
    return map_hash_cmd(a.contents.cmd);
  default:
    return 0;
  }
}

/* we need to copy the string to the internal buffer 
 * so it can live beyond the lifetime of command execution
 */
bool map_copy_atom(mish_map* m, mish_atom* dest, mish_atom* source) {
  mish_str source_s;
  mish_str dest_s;
  
  dest->kind = source->kind;
  switch (source->kind) {
    case mish_atk_string:
      source_s = source->contents.string;
      dest_s.buffer = arena_alloc(m->str_arena, source_s.length);
      if (dest_s.buffer == NULL) {
        return false;
      }
      dest_s.length = source_s.length;
      memcpy(dest_s.buffer, source_s.buffer, source_s.length);
      dest->contents.string = dest_s;
      return true;
    case mish_atk_exact_num:
      dest->contents.exact_num = source->contents.exact_num;
      return true;
    case mish_atk_inexact_num:
      dest->contents.inexact_num = source->contents.inexact_num;
      return true;
    case mish_atk_command:
      dest->contents.cmd = source->contents.cmd;
      return true;
    default:
      return false;
  }
}


/*
 * Inserts a key-value pair into the map.
 * 
 * If the key already exists, insertion is aborted (no update is allowed).
 * This implementation uses a linked list for collision resolution
 *
 * Modifying anything in the environment means lifetimes are not linear
 * ie: replacing a string would not clear the space occupied by
 * the previous one, this is impossible since we only use arena allocators.
 * For this reason, we disallow updates.
 */
bool map_insert(mish_map* m, mish_atom key, mish_atom value) {
  int index = map_hash(key) % m->num_buckets;
  mish_atom_list* list = &(m->buckets[index]);
  mish_list_node* n = list->head;
  while (n != NULL) {
    if (mish_atom_equals(key, n->key)) {
      return false; 
    }
    n = n->next;
  }

  n = arena_alloc(m->node_arena, sizeof(mish_list_node));
  if (n == NULL) {
    return false;
  }
  n->next = NULL;
  if (map_copy_atom(m, &(n->key), &key)     == false ||
      map_copy_atom(m, &(n->value), &value) == false) {
    return false;
  }

  if (list->head == NULL) {
    list->head = n;
    list->tail = n;
    return true;
  }

  list->tail->next = n;
  list->tail = n;
  return true;
}

bool map_find(mish_map* m, mish_atom key, mish_atom* out) {
  int index = map_hash(key) % m->num_buckets;
  mish_atom_list list = m->buckets[index];
  mish_list_node* n = list.head;
  while (n != NULL) {
    if (mish_atom_equals(key, n->key)) {
      *out = n->value;
      return true;
    }
    n = n->next;
  }
  return false;
}

void map_clear(mish_map* m) {
  size_t i;
  mish_atom_list* item;
  for (i = 0; i < m->num_buckets; i++) {
    item = &(m->buckets[i]);
    item->head = NULL;
    item->tail = NULL;
  }
  arena_free_all(m->str_arena);
  arena_free_all(m->node_arena);
}

bool map_is_empty(mish_map* m) {
  size_t i;
  mish_atom_list item;
  for (i = 0; i < m->num_buckets; i++) {
    item = m->buckets[i];
    if (item.head != NULL || item.tail != NULL) {
      return false;
    }
  }
  return arena_empty(m->str_arena) &&
         arena_empty(m->node_arena);
}
/* END: MAP NAMESPACE */

/* BEGIN: LEX NAMESPACE */
/* This implements a simple lexer, most of the parsing
 * actually occurs here, since the microsyntax is more
 * complicated than the macrosyntax.
 */
typedef enum {
  lex_kind_bad,
  lex_kind_num,
  lex_kind_str,
  lex_kind_colon,
  lex_kind_id,
  lex_kind_dollar,
  lex_kind_newline,
  lex_kind_eof
} lex_kind;

typedef enum {
  lex_valkind_none,
  lex_valkind_exact_num,
  lex_valkind_inexact_num
} lex_valkind;

typedef union {
  uint64_t exact_num;
  double   inexact_num;
} lex_value;

typedef struct {
  lex_kind kind;
  lex_valkind vkind;

  size_t begin;
  size_t end;

  lex_value value;
} lex_lexeme;

/* cheaper than strlen */
int lex_lexeme_len(lex_lexeme l) {
  return l.end - l.begin;
}

/* hopefully will be inlined */
char* lex_lexeme_str(const char* input, lex_lexeme l) {
  return (char*)(input + l.begin);
}

typedef struct {
  const char* input;
  size_t input_size;
  lex_lexeme lexeme;
  mish_error err;
} lex;

lex lex_new(const char* input, size_t size) {
  lex l;
  l.input = input;
  l.input_size = size;
  l.lexeme.begin = 0;
  l.lexeme.end = 0;
  l.lexeme.vkind = lex_valkind_none;
  l.lexeme.kind = lex_kind_bad;
  l.err.code = mish_error_none;
  return l;
}

void lex_print_lexeme(lex* l) {
  printf("{begin: %ld, end: %ld, kind: %d, text: \"%.*s\"}\n",
         (long int)l->lexeme.begin,
         (long int)l->lexeme.end,
         (int)l->lexeme.kind,
         lex_lexeme_len(l->lexeme),
         lex_lexeme_str(l->input, l->lexeme));
}

mish_error lex_base_err(lex* l) {
  mish_error err;
  err.code = mish_error_none;
  err.range.begin = l->lexeme.begin;
  err.range.end = l->lexeme.end;
  return err;
}

mish_error lex_err(lex* l, mish_error_code code) {
  mish_error err = lex_base_err(l);
  err.code = code;
  return err;
}

mish_error lex_err_internal(lex* l) {
  mish_error err = lex_base_err(l);
  err.code = mish_error_internal_lexer;
  return err;
}

utf8_rune lex_next_rune(lex* l) {
  utf8_rune r;
  size_t size;
  const char* decode_start;
  size_t remaining_buffer;

  if (l->lexeme.end >= l->input_size) {
    return utf8_EoF;
  }
  
  decode_start = l->input + l->lexeme.end;
  remaining_buffer = l->input_size - l->lexeme.end;
  size = utf8_decode(decode_start, remaining_buffer, &r);
  if (size == 0 || r == -1) {
    l->err = lex_err(l, mish_error_bad_rune);
    return -1;
  }
  l->lexeme.end += size;
  return r;
}

utf8_rune lex_peek_rune(lex* l) {
  utf8_rune r;
  size_t size;
  const char* decode_start;
  size_t remaining_buffer;
  if (l->lexeme.end >= l->input_size) {
    return utf8_EoF;
  }

  decode_start = l->input + l->lexeme.end;
  remaining_buffer = l->input_size - l->lexeme.end;
  size = utf8_decode(decode_start, remaining_buffer, &r);
  
  if (size == 0 || r == -1) {
    l->err = lex_err(l, mish_error_bad_rune);
    return -1;
  }
  return r;
}

void lex_ignore(lex* l) {
  l->lexeme.begin = l->lexeme.end;
  l->lexeme.kind = lex_kind_bad;
}

typedef bool (*lex_validator)(utf8_rune);

bool lex_is_decdigit(utf8_rune r) {
  return (r >= '0' && r <= '9') ||
         (r == '_');
}

bool lex_is_hexdigit(utf8_rune r) {
  return (r >= '0' && r <= '9') ||
         (r >= 'a' && r <= 'f') ||
         (r >= 'A' && r <= 'F') ||
         (r == '_');
}

bool lex_is_bindigit(utf8_rune r) {
  return (r == '0') ||
         (r == '1') ||
         (r == '_');
}

bool lex_is_idchar(utf8_rune r) {
  return (r >= 'a' && r <= 'z') ||
         (r >= 'A' && r <= 'Z') ||
         (r == '~') || (r == '+') ||
         (r == '-') || (r == '_') ||
         (r == '*') || (r == '/') ||
         (r == '?') || (r == '=') ||
         (r == '&') || 
         (r == '%') || (r == '<') ||
         (r == '>') || (r == '!');
}

bool lex_is_idcharnum(utf8_rune r) {
  return (r >= '0' && r <= '9') ||
         (r >= 'a' && r <= 'z') ||
         (r >= 'A' && r <= 'Z') ||
         (r == '~') || (r == '+') ||
         (r == '-') || (r == '_') ||
         (r == '*') || (r == '/') ||
         (r == '?') || (r == '=') ||
         (r == '&') || 
         (r == '%') || (r == '<') ||
         (r == '>') || (r == '!');
}

bool lex_is_whitespace(utf8_rune r) {
  return (r == ' ') || (r == '\r') || (r == '\t');
}

bool lex_is_special_dq_str_char(utf8_rune r) {
  return (r == '\\') || (r == '"');
}

bool lex_is_special_sq_str_char(utf8_rune r) {
  return (r == '\\') || (r == '\'');
}

bool lex_accept_run(lex* l, lex_validator v) {
  utf8_rune r = lex_peek_rune(l);
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

bool lex_accept_until(lex* l, lex_validator v) {
  utf8_rune r = lex_peek_rune(l);
  int i = 0;
  if (r < 0) {
    return false;
  }
  if (r == utf8_EoF) {
    l->err = lex_err(l, mish_error_unexpected_EOF);
    return false;
  }
  while (!v(r)) {
    lex_next_rune(l);
    r = lex_peek_rune(l);
    if (r < 0) {
      return false;
    }
    if (r == utf8_EoF) {
      l->err = lex_err(l, mish_error_unexpected_EOF);
      return false;
    }
    i++;
  }

  if (r == utf8_EoF) {
    l->err.code = mish_error_unexpected_EOF;
    return false;
  }

  return true;
}

bool lex_read_strlit(lex* l, char delim) {
  utf8_rune r = lex_peek_rune(l);
  bool ok;
  lex_validator val;
  if (r != delim) {
    l->err = lex_err(l, mish_error_internal_lexer);
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
      l->lexeme.kind = lex_kind_str;
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

bool lex_conv_hex(lex* l, uint64_t* value) {
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
      l->err = lex_err(l, mish_error_internal_lexer);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_bin(lex* l, uint64_t* value) {
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
      l->err = lex_err(l, mish_error_internal_lexer);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_dec(lex* l, uint64_t* value) {
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
      l->err = lex_err(l, mish_error_internal_lexer);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_conv_inexact(lex* l, double* value) {
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
      l->err = lex_err(l, mish_error_internal_lexer);
      return false;
    }
    begin++;
  }
  *value = output;
  return true;
}

bool lex_read_number(lex* l) {
  utf8_rune r = lex_peek_rune(l);
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
        l->lexeme.vkind = lex_valkind_exact_num;
        l->lexeme.kind = lex_kind_num;
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
        l->lexeme.vkind = lex_valkind_exact_num;
        l->lexeme.kind = lex_kind_num;
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
    l->lexeme.vkind = lex_valkind_inexact_num;
    l->lexeme.kind = lex_kind_num;
  } else {
    ok = lex_conv_dec(l, &exact_value);
    if (ok == false) {
      return false;
    }
    l->lexeme.value.exact_num = exact_value;
    l->lexeme.vkind = lex_valkind_exact_num;
    l->lexeme.kind = lex_kind_num;
  }
  return true;
}

bool lex_read_identifier(lex* l) {
  utf8_rune r = lex_peek_rune(l);
  bool ok;
  if (lex_is_idchar(r) == false){
    l->err = lex_err(l, mish_error_internal_lexer);
    return false;
  }
  l->lexeme.kind = lex_kind_id;
  ok = lex_accept_run(l, lex_is_idcharnum);
  if (ok == false) {
    return false;
  }
  return true;
}

bool lex_ignore_whitespace(lex* l) {
  utf8_rune r = lex_peek_rune(l);
  if (r < 0) {
    return false;
  }
  while (true) {
    if (lex_is_whitespace(r)) {
      lex_next_rune(l);
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

bool lex_read_any(lex* l) {
  utf8_rune r;
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
      l->lexeme.kind = lex_kind_colon;
      break;
    case '$':
      lex_next_rune(l);
      l->lexeme.kind = lex_kind_dollar;
      break;
    case '\n':
      lex_next_rune(l);
      l->lexeme.kind = lex_kind_newline;
      break;
    case utf8_EoF:
      l->lexeme.kind = lex_kind_eof;
      break;
    default:
      l->err = lex_err(l, mish_error_unrecognized_runer);
      return false;
  }
  return true;
}

/*
 * it returns true if everything went smoothly,
 * and returns false if there was some error.
 * the error is stored in lexer.err
 */
bool lex_next(lex* l) {
  l->lexeme.begin = l->lexeme.end;
  return lex_read_any(l);
}
/* END: LEX NAMESPACE */


/* BEGIN: PAR NAMESPACE */
/* parser is technically recursive descent, but without
 * any recursion, since the grammar is not recursive
 *
 * besides that, the parser also retrieves information from
 * the environment, so that it's not only parsing, but
 * also name resolution
 */
mish_str par_create_string(lex* l) {
  mish_str s;
  s.length = lex_lexeme_len(l->lexeme) -2; /* minus delimiters */
  s.buffer = lex_lexeme_str(l->input, l->lexeme) + 1; /* jump first delimiter */
  return s;
}

mish_str par_create_string_from_id(lex* l) {
  mish_str s;
  s.length = lex_lexeme_len(l->lexeme);
  s.buffer = lex_lexeme_str(l->input, l->lexeme);
  return s;
}

bool par_create_atom(lex* l, mish_shell* ctx, mish_atom* a) {
  bool ok;

  switch (l->lexeme.kind) {
    case lex_kind_str:
      a->kind = mish_atk_string;
      a->contents.string = par_create_string(l);
      break;
    case lex_kind_id:
      a->kind = mish_atk_string;
      a->contents.string = par_create_string_from_id(l);
      break;
    case lex_kind_num:
      switch (l->lexeme.vkind) {
      case lex_valkind_exact_num:
        a->kind = mish_atk_exact_num;
        a->contents.exact_num = l->lexeme.value.exact_num;
        break;
      case lex_valkind_inexact_num:
        a->kind = mish_atk_inexact_num;
        a->contents.inexact_num = l->lexeme.value.inexact_num;
        break;
      default:
        return false;
      }
      break;
    default:
      return false;
  }
  ok = lex_next(l);
  if (!ok) {
    ctx->err = l->err;
    return false;
  }
  return true;
}

bool par_eval_variable(mish_shell* ctx, mish_atom* a) {
  return map_find(&ctx->map, *a, a);
}

/* TODO: verify if errors are good */
/* Atom = ['$'] (id | num | str). */
bool par_parse_atom(lex* l, mish_atom* a, mish_shell* ctx) {
  bool is_var = false;
  bool ok;

  switch (l->lexeme.kind) {
    case lex_kind_newline:
      return false;
    case lex_kind_eof:
      return false;
    default:
      break;
  }

  if (l->lexeme.kind == lex_kind_dollar) {
    is_var = true;

    ok = lex_next(l);
    if (!ok) {
      ctx->err = l->err;
      return false;
    }
  }

  if (par_create_atom(l, ctx, a) == false) {
    ctx->err = lex_err(l, mish_error_internal_parser);
    return false;
  }

  if (is_var) {
    ok = par_eval_variable(ctx, a);
    if (!ok) {
      ctx->err.code = mish_error_variable_not_found;
      return false;
    }
  }

  return true;
}

/* Pair = Atom [':' Atom]. */
bool par_parse_arg(lex* l, mish_shell* ctx, mish_argument* arg) {
  mish_atom at1;
  mish_atom at2;
  mish_pair p;
  bool ok;

  if (par_parse_atom(l, &at1, ctx) == false) {
    return false;
  }

  if (l->lexeme.kind == lex_kind_colon) {
    ok = lex_next(l);
    if (!ok) {
      ctx->err = l->err;
      return false;
    }

    if (par_parse_atom(l, &at2, ctx) == false) {
      return false;
    }

    p.key = at1;
    p.value = at2;
    arg->kind = mish_ark_pair;
    arg->contents.pair = p;
    return true;
  }
  arg->kind = mish_ark_atom;
  arg->contents.atom = at1;
  return true;
}

bool par_parse_cmd(lex* l, mish_shell* ctx, mish_argument* arg) {
  mish_atom a;
  bool ok;
  arg->kind = mish_ark_atom;

  if (par_create_atom(l, ctx, &a) == false) {
    ctx->err = lex_err(l, mish_error_internal_parser);
    return false;
  }

  ok = par_eval_variable(ctx, &a);
  if (!ok) {
    ctx->err.code = mish_error_variable_not_found;
    return false;
  }
  
  arg->contents.atom = a;
  return true;
}

/* Command = id {Pair} '\n'. */
mish_arg_list* par_parse(char* input, size_t input_size, mish_shell* ctx) {
  lex l = lex_new(input, input_size);
  mish_arg_list* root;
  mish_arg_list* list;
  mish_argument arg;
  bool ok;
  ctx->err.code = mish_error_none;

  arena_free_all(ctx->arg_arena);

  ok = lex_next(&l);
  if (!ok) {
    ctx->err = l.err;
    return NULL;
  }

  if (l.lexeme.kind != lex_kind_id) {
    ctx->err = lex_err(&l, mish_error_expected_command);
    return NULL;
  }

  list = (mish_arg_list*) arena_alloc(ctx->arg_arena, sizeof(mish_arg_list));
  if (list == NULL) {
    ctx->err = lex_err(&l, mish_error_parser_out_of_memory);
    return NULL;
  }
  root = list;

  ok = par_parse_cmd(&l, ctx, &arg);
  if (!ok && ctx->err.code != mish_error_none) {
    return NULL;
  }

  list->arg = arg;
  
  while (par_parse_arg(&l, ctx, &arg)) {
    list->next = (mish_arg_list*) arena_alloc(ctx->arg_arena, sizeof(mish_arg_list));
    if (list->next == NULL) {
      ctx->err = lex_err(&l, mish_error_parser_out_of_memory);
      return NULL;
    }

    list = list->next;
    list->arg = arg;
  }

  if (ctx->err.code != mish_error_none) {
    return NULL;
  }
  list->next = NULL;
  return root;
}
/* END: PAR NAMESPACE */

/* BEGIN: SHELL NAMESPACE */
size_t mish_shell_write_atom(mish_shell* s, mish_atom a) {
  size_t offset = mish_snprint_atom(s->out_buffer + s->written, s->buff_size - s->written, a);
  s->written += offset;
  return offset;
}

size_t mish_shell_write_arg(mish_shell* s, mish_argument a) {
  size_t offset = mish_snprint_arg(s->out_buffer + s->written, s->buff_size - s->written, a);
  s->written += offset;
  return offset;
}

size_t mish_shell_write_strlit(mish_shell* s, char* string) {
  size_t offset = snprintf(s->out_buffer + s->written, s->buff_size - s->written, "%s", string);
  s->written += offset;
  return offset;
}

size_t mish_shell_write_char(mish_shell* s, char c) {
  if (s->written >= s->buff_size) {
    return 0;
  }
  s->out_buffer[s->written] = c;
  s->written ++;
  return 1;
}

bool mish_shell_new_cmd(mish_shell* s, char* name, mish_command cmd) {
  return map_insert(&s->map, mish_atom_create_str(name), mish_atom_create_cmd(cmd));
}

bool shell_assert_config() {
  size_t total = 0;
  total += MISH_CFG_ARG_ARENA_SIZE;
  total += MISH_CFG_STR_ARENA_SIZE;
  total += MISH_CFG_NODE_ARENA_SIZE;
  total += MISH_CFG_HASHMAP_BUCKET_ARRAY_SIZE;
  total += MISH_CFG_OUT_BUFFER_SIZE;
  return total == MISH_CFG_GRANULARITY;
}

size_t shell_compute_size(size_t total_size, size_t ratio) {
  size_t region_size = util_align_trim_down((total_size*ratio)/MISH_CFG_GRANULARITY);
  return region_size;
}

mish_error_code mish_shell_new(uint8_t* buffer, size_t size, mish_shell* s) {
  uint8_t* start;
  size_t region_size;
  arena_RES res;
  mish_error_code err = mish_error_none;
  s->err.code = mish_error_none;

  if (shell_assert_config() == false) {
    return mish_error_bad_memory_config;
  }

  start = buffer;
  region_size = shell_compute_size(size, MISH_CFG_ARG_ARENA_SIZE);
  s->arg_arena = arena_new(start, region_size, &res);
  if (res != arena_OK) {
    return arena_map_res(res);
  }

  start += region_size;
  region_size = shell_compute_size(size, MISH_CFG_STR_ARENA_SIZE);
  s->map.str_arena = arena_new(start, region_size, &res);
  if (res != arena_OK) {
    return arena_map_res(res);
  }

  start += region_size;
  region_size = shell_compute_size(size, MISH_CFG_NODE_ARENA_SIZE);
  s->map.node_arena = arena_new(start, region_size, &res);
  if (res != arena_OK) {
    return arena_map_res(res);
  }

  start += region_size;
  region_size = shell_compute_size(size, MISH_CFG_HASHMAP_BUCKET_ARRAY_SIZE);
  s->map.buckets = (mish_atom_list*)start;
  s->map.num_buckets = region_size / sizeof(mish_atom_list);

  start += region_size;
  region_size = shell_compute_size(size, MISH_CFG_OUT_BUFFER_SIZE);
  s->out_buffer = (char*)start;
  s->buff_size = region_size;
  s->written = 0;

  mish_builtin_hard_clear(s, NULL);

  return err;
}

mish_error_code mish_shell_eval(mish_shell* s, char* cmd, size_t cmd_size) {
  mish_arg_list* list = par_parse(cmd, cmd_size, s);
  mish_argument arg;
  mish_atom at;
  mish_error_code err;
  if (list == NULL) {
    return s->err.code;
  }
  arg = list->arg;
  if (arg.kind != mish_ark_atom) {
    return mish_error_internal_exp_atom;
  }
  at = arg.contents.atom;
  if (at.kind != mish_atk_command) {
    return mish_error_internal_exp_cmd;
  }

  s->written = 0;
  *(s->out_buffer) = '\0';

  err = (at.contents.cmd)(s, list);
  return err;
}
/* END: SHELL NAMESPACE */

/* BEGIN: ARGVAL NAMESPACE*/
/* definition of functions related to argument validation */

/* returns true if all arguments are pairs,
 * discards the first argument as it is expected to be the
 * command.
 */
bool mish_argval_only_pairs(mish_arg_list* args) {
  mish_arg_list* curr;
  if (args == NULL) {
    return true; /* if there are no arguments, then all of them are pairs :) */
  }
  /* jumping the command */
  curr = args->next;
  while (curr != NULL) {
    if (curr->arg.kind != mish_ark_pair) {
      return false;
    }
    curr = curr->next;
  }
  return true;
}

/* END: ARGVAL NAMESPACE*/

/* BEGIN: BUILTIN NAMESPACE */
mish_error_code mish_builtin_def(mish_shell* s, mish_arg_list* args) {
  mish_arg_list* curr;
  mish_pair p;
  bool ok;

  if (args == NULL) {
    return mish_error_internal;
  }

  /* first we check if arguments are well formed */
  if (mish_argval_only_pairs(args) == false) {
    return mish_error_contract_violation;
  }

  curr = args->next;
  while (curr != NULL) {
    p = curr->arg.contents.pair;

    ok = map_insert(&s->map, p.key, p.value);
    if (!ok) {
      return mish_error_insert_failed;
    }
    
    curr = curr->next;
  }
  return mish_error_none;
}

mish_error_code mish_builtin_echo(mish_shell* s, mish_arg_list* args) {
  mish_arg_list* curr;

  if (args == NULL) {
    return mish_error_internal;
  }

  curr = args->next;
  while (curr != NULL) {
    mish_shell_write_arg(s, curr->arg);
    if (curr->next != NULL) {
      mish_shell_write_strlit(s, "; ");
    }
    curr = curr->next;
  }
  mish_shell_write_strlit(s, ";\n");
  mish_shell_write_char(s, '\0');
  return mish_error_none;
}

/* resets the environment to the default state */
mish_error_code mish_builtin_hard_clear(mish_shell* s, mish_arg_list* args) {
  if (args == NULL) {
    /* avoid warning */
  }
  map_clear(&s->map);
  return mish_error_none;
}
/* END: BUILTIN NAMESPACE */
