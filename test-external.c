/*
  This file tests the shell as an external program would use it,
  it should import only "pami-shell.h" and necessary conveniences.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pami-shell.h"

#define SHELL_MEMORY_SIZE 4096
uint8_t shell_memory[SHELL_MEMORY_SIZE] = {0};

/* BEGIN: EVAL TEST */
error_code cmd_clear(shell* s, arg_list* list) {
  bool ok = true;
  builtin_hard_clear(s, list);
  
  ok = ok && shell_new_cmd(s, "def", builtin_def);
  ok = ok && shell_new_cmd(s, "echo", builtin_echo);
  ok = ok && shell_new_cmd(s, "clear", cmd_clear);
  if (ok == false) {
    return error_insert_failed;
  }
  return error_none;
}

#define NUM_COMMANDS 7
char* commands[NUM_COMMANDS] = {
  "def cmd:i2cscan port:8080\n",
  "echo $cmd $port\n",
  "def a:0xFF b:0b1010\n",
  "echo $a $b\n",
  "clear\n",
  "def a:\"\x68\U00000393\U000030AC\U000101FA\"\n",
  "echo $a\n",
};
char* expected[NUM_COMMANDS] = {
  "",
  "\"i2cscan\"; 8080;\n",
  "",
  "255; 10;\n",
  "",
  "",
  "\"\x68\U00000393\U000030AC\U000101FA\";\n",
};

void eval_once(shell* s, char* cmd) {
  size_t size;
  error_code err;

  printf("> %s", cmd);

  size = strlen(cmd);
  err = shell_eval(s, cmd, size);
  if (err != error_none) {
    printf("error: %d\n", err);
    abort();
  }

  printf("%.*s\n", (int)s->written, s->out_buffer);
}

void eval_test() {
  shell s;
  error_code err;
  int i;
  char* cmd; char* exp;
  printf(">>>>>>>>>>>> EVAL TEST\n");
  err = shell_new(shell_memory, SHELL_MEMORY_SIZE, &s);
  if (err != error_none) {
    printf("error: %d\n", err);
    abort();
  }

  err = cmd_clear(&s, NULL);
  if (err != error_none) {
    printf("error: %d\n", err);
    abort();
  }

  for (i = 0; i < NUM_COMMANDS; i++) {
    cmd = commands[i];
    exp = expected[i];
    eval_once(&s, cmd);
    if (strncmp(s.out_buffer, exp, s.written) != 0) {
      printf("invalid response: \"%.*s\"\n != \"%.*s\"\n",
             (int)s.written, s.out_buffer,
             (int)s.written, exp);
      abort();
    }
  }
}
/* END: EVAL TEST */

int main() {
  eval_test();
  return 0;
}
