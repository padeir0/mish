#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct argument;
struct shell;

typedef struct {
  argument* argv;
  int argc;
} arg_list;

typedef void (*command)(struct shell* s, arg_list args);

typedef struct {
  char* buffer;
  size_t length;
} str;

enum atom_kind {
  atk_string,
  atk_exact_num,
  atk_inexact_num,
  atk_id
};

typedef struct {
  union {
    str string;
    uint64_t exact_num;;
    double inexact_num;
    command cmd;
  } contents;
  enum atom_kind kind;
} atom;

enum arg_kind {
  ark_pair,
  ark_atom
};

typedef struct {
  atom key;
  atom value;
} pair;

typedef struct argument {
  enum arg_kind kind;
  union {
    pair pair;
    atom atom;
  } contents;
} argument;

typedef struct _node {
  atom key;
  atom value;
  struct _node* next;
} list_node;

typedef struct {
  list_node* head, tail;
} atom_list;

typedef struct {
  atom_list* buckets;
  size_t num_buckets;
} map;

typedef struct shell {
  map symbols;
} shell;

enum error_code {
  error_none,
  error_contract_violation,
  error_bad_rune,
  error_internal_lexer,
  error_invalid_syntax,
  error_unrecognized_rune,
  error_parser_out_of_memory
};

typedef struct {
  range range;
  enum error_code code;
} error;
