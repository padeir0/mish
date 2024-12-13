/*
  This file tests the shell as an external program would use it,
  it should import only "pami-shell.h" and necessary conveniences.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mish.h"

#define SHELL_MEMORY_SIZE 8192
uint8_t shell_memory[SHELL_MEMORY_SIZE] = {0};

/* BEGIN: EVAL TEST */

mish_error_code cmd_corrupt_print(mish_shell* s, mish_arg_list* list) {
  memset(s->cmd, 'A', s->cmd_size);
  s->cmd[s->cmd_size-1] = '\0';
  printf("source_cmd: %s\n", s->cmd);
  return mish_builtin_echo(s, list);
}

mish_error_code cmd_clear(mish_shell* s, mish_arg_list* list) {
  bool ok = true;
  mish_builtin_hard_clear(s, list);
  
  ok = ok && mish_shell_add_cmd(s, "def", mish_builtin_def);
  ok = ok && mish_shell_add_cmd(s, "echo", mish_builtin_echo);
  ok = ok && mish_shell_add_cmd(s, "bad-echo", cmd_corrupt_print);
  ok = ok && mish_shell_add_cmd(s, "clear", cmd_clear);
  if (ok == false) {
    return mish_error_insert_failed;
  }
  return mish_error_none;
}

#define NUM_COMMANDS 14
char* commands[NUM_COMMANDS] = {
  "def cmd:i2cscan port:8080\r\n",
  "echo $cmd $port\r\n",
  "def a:0xFF b:0b1010\r\n",
  "echo $a $b\r\n",
  "clear\r\n",
  "def a:\"\x68\U00000393\U000030AC\U000101FA\"\r\n",
  "echo $a\r\n",
  "bad-echo pipipi:popopo\r\n",
  "clear\r\n",
  "echo a:1 b:2 | def c:3\r\n",
  "echo $a $b $c\r\n",
  "clear\r\n",
  "echo a:1 b:2 | echo c:3 d:4 e:5 | def f:6\r\n",
  "echo $a $b $c $d $e $f\r\n"
};
char* expected[NUM_COMMANDS] = {
  "",
  "\"i2cscan\" 8080 \r\n",
  "",
  "255 10 \r\n",
  "",
  "",
  "\"\x68\U00000393\U000030AC\U000101FA\" \r\n",
  "\"pipipi\":\"popopo\" \r\n",
  "",
  "",
  "1 2 3 \r\n",
  "",
  "",
  "1 2 3 4 5 6 \r\n",
};

void eval_once(mish_shell* s, char* cmd) {
  size_t size;
  mish_error_code err;

  printf("> %s", cmd);

  size = strlen(cmd);
  err = mish_shell_eval(s, cmd, size);
  if (err != mish_error_none) {
    mish_builtin_print_env(s, NULL);
    printf("{%.*s}\n", (int)s->written, s->out_buffer);
    
    printf("error: %s\n", mish_util_error_str(err));
    abort();
  }

  printf("%.*s\n", (int)s->written, s->out_buffer);
}

char scratch_buff[256] = {0};

void eval_test() {
  mish_shell s;
  mish_error_code err;
  int i;
  char* cmd; char* exp;
  printf(">>>>>>>>>>>> EVAL TEST\n");
  err = mish_shell_new(shell_memory, SHELL_MEMORY_SIZE, &s);
  if (err != mish_error_none) {
    printf("error: %s\n", mish_util_error_str(err));
    abort();
  }

  err = cmd_clear(&s, NULL);
  if (err != mish_error_none) {
    printf("error: %s\n", mish_util_error_str(err));
    abort();
  }

  for (i = 0; i < NUM_COMMANDS; i++) {
    cmd = commands[i];
    exp = expected[i];
    memcpy(scratch_buff, cmd, strlen(cmd)+1);
    eval_once(&s, scratch_buff);
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
