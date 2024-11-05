#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pami-shell.c"

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
  char* curr_char = utf8_test_data;
  check_rune(&curr_char, 0x68);
  check_rune(&curr_char, 0x0393);
  check_rune(&curr_char, 0x30AC);
  check_rune(&curr_char, 0x101FA);
  printf("utf8_test: OK\n");
}

char lex_test_data[] = "cmd a:abcde b:123 c:0b101 d:0xCAFE e:123.0 f:\"\x68\U00000393\U000030AC\U000101FA\"\n";

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

int main() {
  utf8_test();

  printf("%s", lex_test_data);
  lexer l = lex_new_lexer(lex_test_data, strlen(lex_test_data));
  
  while (lex_next(&l) && l.lexeme.kind != lk_eof) {
    print_lexeme(l.input, l.lexeme);
  }

  if (l.lexeme.kind != lk_eof) {
    printf("lex error at %d:%d number %d\n", l.err.range.begin, l.err.range.end, l.err.code);
    abort();
  }

  return 0;
}
