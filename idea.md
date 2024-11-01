# Pami-Shell

Minimal shell for microcontrollers. It is only a way to call
user-written C functions that do the actual work.
Most of all, it must be small.

## Grammar

```ebnf
Command = id {Pair} '\n'.
Pair = Atom [':' Atom].
Atom = id | num | str.

num = hex | bin | dec.
dec = dec-num ['.' dec-num].
dec-num = dec-digit {dec-digit}.
hex = '0x' hex-digit {hex-digit}.
bin = '0b' bin-digit {bin-digit}.
dec-digit = /[0-9_]/.
bin-digit = /[01_]/.
hex-digit = /[0-9A-Fa-f_]/.

str = str-double | str-single.
str-double = '"' {inside-str-double} '"'.
str-single = "'" {inside-str-single} "'".
inside-str-double = utf8 | double-escapes.
inside-str-single = utf8 | single-escapes.
utf8 = /[\u0000-\uFFFF]/.
double-escapes = '\r' | '\n' | '\t' | '"'.
single-escapes = '\r' | '\n' | '\t' | '\''.

id = id-char {id-char-num}.
id-char = letters |
     '~' | '+' | '-' | '_' | '*' | '/' | '?' | '=' |
     '&' | '$' | '%' | '<' | '>' | '!'.
id-char-num = id-char | digit.



```

## Examples

```
> wifi-connect ssid:Embeddona pwd:31415926
> show-tasks
wifi  main  imu
> set-name Sensor-1
```

## Limitations

The shell interface cannot be used directly to watch tasks or sensors,
since that would require the shell to handle termination signals and
that would either be too complex or be RTOS-dependent. The work-around
to this limitation is to use other tasks and data streams to receive the data.
For example, to watch one IMU sensor, we can call a task that expects
an ip address with port, where the microcontroller will send packets
of data.

```
> watch-imu start addr:"192.168.0.1:8080" sample-rate:20
```

Then the user can open a new terminal on his Linux computer
using `netcat` or a similar program, and with that he would be able
to properly watch the data stream. Then, to stop sending the data,
another command can be issued.

```
> watch-imu stop
```

Which will stop the data stream.

This approach is simple and does not require special signal handling.
However, it is necessary that all commands are non-blocking, or at least
have a proper timeout, so that further commands can be issued. 

## Format of arguments

```c
#include <stdint.h>
#include <stddef.h>

struct argument;
struct shell;

typedef void (*command)(struct shell* s, struct argument* argv, int argc);

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
```

## Memory management

Arguments are parsed and inserted into an arena allocator,
they are copied by value and the arena is freed once the
command finishes execution.

Any insertion on the environment map results in a copy of the argument
to the map internal memory. The map is managed by a free-list allocator
and memory is only freed when items are removed from the map.

This means no garbage collection is necessary.
