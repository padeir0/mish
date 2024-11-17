#ifdef __cplusplus
extern "C" {
#endif

#include <mish.h>

#ifdef __cplusplus
}
#endif

#define MSG_SIZE 512
char received_message[MSG_SIZE] = {0};

#define SHELL_MEMORY_SIZE 8196
uint8_t shell_memory[SHELL_MEMORY_SIZE] = {0};
int i = 0;
mish_shell s;

mish_error_code cmd_setgpio(mish_shell* s, mish_arg_list* args) {
  mish_arg_list* curr;
  mish_pair p;
  int64_t gpio;
  int64_t value;

  if (args == NULL) {
    return mish_error_internal;
  }

  if (mish_argval_only_pairs(args) == false) {
    return mish_error_contract_violation;
  }

  curr = args->next;
  while (curr != NULL) {
    p = curr->arg.contents.pair;

    if (mish_atom_is_exact(p.key) == false ||
        mish_atom_is_exact(p.value) == false) {
      return mish_error_contract_violation;
    }

    gpio = p.key.contents.exact_num;
    value = p.value.contents.exact_num;

    pinMode((uint8_t)gpio, OUTPUT);
    digitalWrite((uint8_t)gpio, (uint8_t) value);

    curr = curr->next;
  }
  return mish_error_none;
}

mish_error_code cmd_clear(mish_shell* s, mish_arg_list* list) {
  bool ok = true;
  mish_builtin_hard_clear(s, list);
  
  ok = ok && mish_shell_new_cmd(s, "def", mish_builtin_def);
  ok = ok && mish_shell_new_cmd(s, "echo", mish_builtin_echo);
  ok = ok && mish_shell_new_cmd(s, "clear", cmd_clear);
  ok = ok && mish_shell_new_cmd(s, "set-gpio", cmd_setgpio);
  if (ok == false) {
    return mish_error_insert_failed;
  }
  return mish_error_none;
}

void setup() {
  mish_error_code err;
  Serial.begin(115200);
  Serial.print(">");
  err = mish_shell_new(shell_memory, SHELL_MEMORY_SIZE, &s);
  if (err != mish_error_none) {
    Serial.printf("mish_shell_new error: %d\n", err);
    abort();
  }
  err = cmd_clear(&s, NULL);
  if (err != mish_error_none) {
    Serial.printf("cmd_clear error: %d\n", err);
    abort();
  }
  uint64_t a;
}

mish_error_code loop_err;
void loop() {
  while (Serial.available() && i < MSG_SIZE-2) {
    char c = Serial.read();
    if (c == '\n') {
      received_message[i] = '\n';
      received_message[i+1] = '\0';
      Serial.print(received_message);

      loop_err = mish_shell_eval(&s, received_message, i+1);
      if (loop_err != mish_error_none) {
        Serial.printf("Error: %d\n", loop_err);
        i = 0;
        *received_message = '\0';
        Serial.print(">");
        continue;
      }

      Serial.print(s.out_buffer);
      Serial.print(">");
      i = 0;
      *received_message = '\0';
    } else {
      received_message[i] = c;
      i++;
    }
  }
}
