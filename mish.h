#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* All public symbols start with "mish",
 * private names will omit this. */

/* BEGIN: CONFIG */

/* This defines how some given memory is split between
 * different regions.
 * The size of a region is defined as:
 *   region_size = (total_memory * CFG_REGION_SIZE) / CFG_GRANULARITY
 */

#define MISH_CFG_GRANULARITY               128
#define MISH_CFG_ARG_ARENA_SIZE            16
#define MISH_CFG_NODE_ARENA_SIZE           32
#define MISH_CFG_HASHMAP_BUCKET_ARRAY_SIZE 8
#define MISH_CFG_OUT_BUFFER_SIZE           24
#define MISH_CFG_STR_ARENA_SIZE            48

/* END: CONFIG*/
typedef enum {
  mish_error_none,
  mish_error_bad_rune,
  mish_error_unexpected_EOF,
  mish_error_invalid_syntax,
  mish_error_unrecognized_rune,
  mish_error_parser_out_of_memory, /* 5 */
  mish_error_expected_command,
  mish_error_arena_null_buffer,
  mish_error_arena_too_small,
  mish_error_variable_not_found,
  mish_error_insert_failed, /* 10 */
  mish_error_contract_violation,
  mish_error_internal,
  mish_error_internal_lexer,
  mish_error_internal_parser,
  mish_error_internal_exp_atom,  /* 15 */
  mish_error_internal_exp_cmd,
  mish_error_bad_memory_config,
  mish_error_cmd_failure
} mish_error_code;


typedef struct {
  int begin, end;
} mish_range;

typedef struct {
  mish_range range;
  mish_error_code code;
} mish_error;

typedef struct {
  char* buffer;
  size_t length;
} mish_str;

typedef enum {
  mish_atk_string,
  mish_atk_exact_num,
  mish_atk_inexact_num,
  mish_atk_command
} mish_atom_kind;

struct mish__arg_list;
struct mish__shell;
typedef mish_error_code (*mish_command)(struct mish__shell* s, struct mish__arg_list* args);

/* TODO: use named commands so that we can properly print them */
typedef struct {
  mish_str name;
  mish_command cmd;
} mish_named_cmd;

typedef struct {
  union {
    mish_str string;
    uint64_t exact_num;;
    double inexact_num;
    mish_command cmd;
  } contents;
  mish_atom_kind kind;
} mish_atom;

typedef enum {
  mish_ark_pair,
  mish_ark_atom
} mish_arg_kind;

typedef struct {
  mish_atom key;
  mish_atom value;
} mish_pair;

typedef struct mish_argument {
  mish_arg_kind kind;
  union {
    mish_pair pair;
    mish_atom atom;
  } contents;
} mish_argument;

/* we use a linked list since we build
 * argument lists dynamically and we can't
 * assume our allocator pads our objects
 * like the C standard requires.
 */
typedef struct mish__arg_list {
  struct mish_argument arg;
  struct mish__arg_list* next;
} mish_arg_list;

/* some of these things should be private */

typedef struct _node {
  mish_atom key;
  mish_atom value;
  struct _node* next;
} mish_list_node;

typedef struct {
  uint8_t* buffer;
  size_t   buffsize;
  size_t   allocated;
} mish_arena;

typedef struct {
  mish_list_node* head;
  mish_list_node* tail;
} mish_atom_list;

typedef struct {
  mish_atom_list* buckets;
  size_t num_buckets;

  mish_arena* str_arena;
  mish_arena* node_arena;
} mish_map;

typedef struct mish__shell {
  mish_map map;
  mish_arena* arg_arena;
  mish_error err;

  char* cmd;
  size_t cmd_size;
  
  char* out_buffer;
  size_t written;
  size_t buff_size;
} mish_shell;

mish_error_code mish_shell_new(uint8_t* buffer, size_t size, mish_shell* s);
mish_error_code mish_shell_eval(mish_shell* s, char* cmd, size_t cmd_size);
size_t mish_shell_write_atom(mish_shell* s, mish_atom a);
size_t mish_shell_write_arg(mish_shell* s, mish_argument a);
size_t mish_shell_write_strlit(mish_shell* s, char* string);
size_t mish_shell_write_char(mish_shell* s, char c);

bool mish_shell_add_atom_cmd(mish_shell* s, mish_atom a, mish_command cmd);
bool mish_shell_add_cmd(mish_shell* s, char* name, mish_command cmd);
bool mish_shell_add_str(mish_shell* s, char* name, char* str);
bool mish_shell_add_exact(mish_shell* s, char* name, int64_t num);
bool mish_shell_add_inexact_num(mish_shell* s, char* name, double num);

size_t mish_shell_available_env_memory(mish_shell* s);

mish_error_code mish_builtin_hard_clear(mish_shell* s, mish_arg_list* list);
mish_error_code mish_builtin_echo(mish_shell* s, mish_arg_list* list);
mish_error_code mish_builtin_def(mish_shell* s, mish_arg_list* list);
mish_error_code mish_builtin_available_env_memory(mish_shell* s, mish_arg_list* list);
mish_error_code mish_builtin_print_env(mish_shell* s, mish_arg_list* list);

mish_atom mish_atom_create_num_exact(uint64_t value);
mish_atom mish_atom_create_num_inexact(double value);
mish_atom mish_atom_create_str(char* s);
mish_atom mish_atom_create_cmd(mish_command cmd);
bool mish_atom_equals(mish_atom a, mish_atom b);
bool mish_atom_is_exact(mish_atom a);
bool mish_atom_is_inexact(mish_atom a);
bool mish_atom_is_cmd(mish_atom a);
bool mish_atom_is_str(mish_atom a);

size_t mish_snprint_atom(char* buffer, size_t size, mish_atom a);
size_t mish_snprint_pair(char* buffer, size_t size, mish_pair p);
size_t mish_snprint_arg(char* buffer, size_t size, mish_argument a);
size_t mish_snprint_arg_list(char* buffer, size_t size, mish_arg_list* list);

bool mish_argval_only_pairs(mish_arg_list* args);

char* mish_util_error_str(mish_error_code code);
