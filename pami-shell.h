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
#define CFG_NODE_ARENA_SIZE           16
#define CFG_HASHMAP_BUCKET_ARRAY_SIZE 16
#define CFG_OUT_BUFFER_SIZE           32
#define CFG_STR_ARENA_SIZE            48

/* END: CONFIG*/
typedef enum {
  error_none,
  error_contract_violation,
  error_bad_rune,
  error_invalid_syntax,
  error_unrecognized_rune,
  error_parser_out_of_memory, /* 5 */
  error_expected_command,
  error_arena_null_buffer,
  error_arena_too_small,
  error_variable_not_found,
  error_insert_failed, /* 10 */
  error_internal,
  error_internal_lexer,
  error_internal_parser,
  error_internal_exp_atom,
  error_internal_exp_cmd /* 15 */
} error_code;

typedef struct {
  int begin, end;
} range;

typedef struct {
  range range;
  error_code code;
} error;

typedef struct {
  char* buffer;
  size_t length;
} str;

typedef enum {
  atk_string,
  atk_exact_num,
  atk_inexact_num,
  atk_command
} atom_kind;

struct arg_list;
struct shell;
typedef error_code (*command)(struct shell* s, struct arg_list* args);

/* TODO: use named commands so that we can properly print them */
typedef struct {
  str name;
  command cmd;
} named_cmd;

typedef struct {
  union {
    str string;
    uint64_t exact_num;;
    double inexact_num;
    command cmd;
  } contents;
  atom_kind kind;
} atom;

typedef enum {
  ark_pair,
  ark_atom
} arg_kind;

typedef struct {
  atom key;
  atom value;
} pair;

typedef struct argument {
  arg_kind kind;
  union {
    pair pair;
    atom atom;
  } contents;
} argument;

/* we use a linked list since we build
 * argument lists dynamically and we can't
 * assume our allocator pads our objects
 * like the C standard requires.
 */
typedef struct arg_list {
  struct argument arg;
  struct arg_list* next;
} arg_list;

/* some of these things should be private */

typedef struct _node {
  atom key;
  atom value;
  struct _node* next;
} list_node;

typedef struct {
  uint8_t* buffer;
  size_t   buffsize;
  size_t   allocated;
} arena;

typedef struct {
  list_node* head;
  list_node* tail;
} atom_list;

typedef struct {
  atom_list* buckets;
  size_t num_buckets;

  arena* str_arena;
  arena* node_arena;
} map;

typedef struct shell {
  map map;
  arena* arg_arena;
  error err;

  char* out_buffer;
  size_t written;
  size_t buff_size;
} shell;

error_code shell_new(uint8_t* buffer, size_t size, struct shell* s);
error_code shell_eval(shell* s, char* cmd, size_t cmd_size);
bool  shell_new_cmd(shell* s, char* name, command cmd);
size_t shell_write_atom(shell* s, atom a);
size_t shell_write_arg(shell* s, argument a);
size_t shell_write_strlit(shell* s, char* string);

error_code builtin_hard_clear(shell* s, arg_list* list);
error_code builtin_echo(shell* s, arg_list* list);
error_code builtin_def(shell* s, arg_list* list);

atom atom_create_num_exact(uint64_t value);
atom atom_create_num_inexact(double value);
atom atom_create_str(char* s);
atom atom_create_cmd(command cmd);
bool atom_equals(atom a, atom b);

size_t snprint_atom(char* buffer, size_t size, atom a);
size_t snprint_arg(char* buffer, size_t size, argument a);
size_t snprint_arg_list(char* buffer, size_t size, arg_list* list);
