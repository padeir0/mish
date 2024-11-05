#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* BEGIN: CONFIG */

/* This defines how some given memory is split between
 * different regions.
 * The size of a region is defined as:
 *   region_size = (total_memory * CFG_REGION_SIZE) / CFG_GRANULARITY
 */

#define CFG_GRANULARITY               128
#define CFG_ARG_ARENA_SIZE            16
#define CFG_STR_ARENA_SIZE            64
#define CFG_NODE_ARENA_SIZE           32
#define CFG_HASHMAP_BUCKET_ARRAY_SIZE 16

/* END: CONFIG*/

struct argument;
struct shell;

enum error_code {
  error_none,
  error_contract_violation,
  error_bad_rune,
  error_internal_lexer,
  error_internal_parser,
  error_invalid_syntax,
  error_unrecognized_rune,
  error_parser_out_of_memory,
  error_expected_command
};

typedef struct {
  int begin, end;
} range;

typedef struct {
  range range;
  enum error_code code;
} error;

typedef struct {
  struct argument* argv;
  int argc;
} arg_list;

typedef error (*command)(arg_list args);

typedef struct {
  char* buffer;
  size_t length;
} str;

enum atom_kind {
  atk_string,
  atk_exact_num,
  atk_inexact_num,
  atk_comand
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
