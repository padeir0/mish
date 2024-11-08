#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pami-shell.c"

/* BEGIN: SHARED */

#define PRINT_BUFFER_SIZE 2048
char print_buffer[PRINT_BUFFER_SIZE] = {0};

#define SHELL_MEMORY_SIZE 8192
uint8_t shell_memory[SHELL_MEMORY_SIZE] = {0};

char cmd1[] = "def cmd:i2cscan port:8080\n";
char cmd2[] = "echo a:abcde b:123 c:0b101 d:0xCAFE e:123.0 f:\"\x68\U00000393\U000030AC\U000101FA\"\n";
/* END: SHARED*/


/* BEGIN: UTF8 TEST */
char* utf8_test_data = "\x68\U00000393\U000030AC\U000101FA";

void check_rune(char** curr_char, rune expected) {
  rune r;
  size_t rune_size;
  rune_size = utf8_decode(*curr_char, &r);
  if (rune_size > 0) {
    *curr_char += rune_size;
    if (r != expected) {
      printf("runes don't match: U+%X != U+%X\n", r, expected);
      abort();
    }
  } else {
    printf("invalid rune, expected U+%X\n", expected);
    abort();
  }
}

/* very weak test, but can be improved later */
void utf8_test() {
  char* curr_char;
  printf(">>>>>>>>>>>> UTF8 TEST\n");

  curr_char = utf8_test_data;
  check_rune(&curr_char, 0x68);
  check_rune(&curr_char, 0x0393);
  check_rune(&curr_char, 0x30AC);
  check_rune(&curr_char, 0x101FA);
  printf("utf8_test: OK\n");
}
/* END: UTF8 TEST */

/* BEGIN: LEX TEST */

void print_lexeme(const char* input, lexeme l) {
  size_t begin = l.begin;
  putchar('"');
  while (begin < l.end) {
    putchar(*(input + begin));
    begin++;
  }
  putchar('"');
  putchar('\n');
}

void lex_test_once(char* s) {
  printf("%s", s);
  lexer l = lex_new_lexer(s, strlen(s));
  
  while (lex_next(&l) && l.lexeme.kind != lk_eof) {
    print_lexeme(l.input, l.lexeme);
  }

  if (l.lexeme.kind != lk_eof) {
    printf("lex error at %d:%d number %d\n", l.err.range.begin, l.err.range.end, l.err.code);
    abort();
  } 
}

void lex_test() {
  printf(">>>>>>>>>>>> LEX TEST\n");
  lex_test_once(cmd1);
  lex_test_once(cmd2);
}
/* END: LEX TEST */

/* BEGIN: MAP TEST */
void print_atom(atom a) {
  size_t offset = 0;
  char* buffer = print_buffer;
  size_t size = PRINT_BUFFER_SIZE;

  offset += snprint_atom(buffer+offset, size-offset, a);

  printf("%.*s", (int)offset, buffer);
}

void print_kvpair(atom key, atom value) {
  size_t offset = 0;
  char* buffer = print_buffer;
  size_t size = PRINT_BUFFER_SIZE;
  offset += snprintf(buffer+offset, size-offset, "(");
  offset += snprint_atom(buffer+offset, size-offset, key);
  offset += snprintf(buffer+offset, size-offset, ", ");
  offset += snprint_atom(buffer+offset, size-offset, value);
  offset += snprintf(buffer+offset, size-offset, ")");

  printf("%.*s", (int)offset, buffer);
}

void print_arg_list(arg_list* list) {
  size_t offset = 0;
  char* buffer = print_buffer;
  size_t size = PRINT_BUFFER_SIZE;

  offset += snprint_arg_list(buffer+offset, size-offset, list);
  printf("%.*s", (int)offset, buffer);
}

#define NUM_MAP_TEST_CASES 8
atom map_test_cases[NUM_MAP_TEST_CASES] = {0};

void fill_test_cases() {
  map_test_cases[0] = atom_create_str("there is");
  map_test_cases[1] = atom_create_str("a house");
  map_test_cases[2] = atom_create_str("in new");
  map_test_cases[3] = atom_create_str("orleans");
  map_test_cases[4] = atom_create_str("they call");
  map_test_cases[5] = atom_create_str("the rising");
  map_test_cases[6] = atom_create_str("sun");
  map_test_cases[7] = atom_create_str("tadadada");
}

void map_test_once(shell* s) {
  atom key;
  atom value;
  atom out;
  bool ok;
  int i;

  for (i = 0; i < NUM_MAP_TEST_CASES; i++) {
    key = map_test_cases[i];
    value = atom_create_num_exact(i);
    ok = map_insert(&s->map, key, value);
    if (!ok) {
      printf("failed to insert: ");
      print_kvpair(key, value);
      printf("\n");
      abort();
    }
  }

  for (i = 0; i < NUM_MAP_TEST_CASES; i++) {
    key = map_test_cases[i];
    value = atom_create_num_exact(i);
    ok = map_find(&s->map, key, &out);
    if (!ok) {
      printf("failed to find: ");
      print_atom(key);
      printf("\n");
      abort();
    }

    if (atom_equals(value, out) == false) {
      printf("value does not match: ");
      print_atom(value);
      printf(" != ");
      print_atom(out);
      printf("\n");
      abort();
    }
  }
}

void map_test() {
  shell s;
  error err;
  int i;

  printf(">>>>>>>>>>>> MAP TEST\n");
  fill_test_cases();
  
  err = new_shell(shell_memory, SHELL_MEMORY_SIZE, &s);
  
  if (err.code != error_none) {
    printf("error: %d\n", err.code);
    abort();
  }

  for (i = 0; i < 5; i++) {
    map_test_once(&s);
    map_clear(&s.map);
    if (map_is_empty(&s.map) == false) {
      printf("fail: map is not empty after clear\n");
      abort();
    }
  }

  printf("success!");
  printf("\n");
}

/* END: MAP TEST */

/* BEGIN: PARSE TEST */

void parse_once(shell* s, char* cmd) {
  arg_list* list;

  list = pr_parse(cmd, strlen(cmd), s);
  if (s->err.code != error_none) {
    printf("parse error ocurred: %d\n", s->err.code);
  }
  print_arg_list(list);
  printf("\n");
}

void parse_test() {
  shell s;
  error err;
  printf(">>>>>>>>>>>> PARSE TEST\n");
  err = new_shell(shell_memory, SHELL_MEMORY_SIZE, &s);
  if (err.code != error_none) {
    printf("error: %d\n", err.code);
    abort();
  }
  parse_once(&s, cmd1);
  parse_once(&s, cmd2);
}

/* END: PARSE TEST */

int main() {
  utf8_test();
  lex_test();
  map_test();
  parse_test();
  return 0;
}
