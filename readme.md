# Mish

Minimal shell for microcontrollers. It is only a way to call
user-written C functions that do the actual work.
Most of all, it must be small.

## Grammar

```ebnf
Command = id {Pair} '\n'.
Pair = Atom [':' Atom].
Atom = ['$'] (id | num | str).

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
     '&' | '%' | '<' | '>' | '!'.
id-char-num = id-char | digit.
```

There are 3 representations of strings:
 - Single quote strings
 - Double quote strings
 - Identifiers

An atom is only evaluated as a variable if
it is preceded by `$`.

Some built-in functions shall be available, but must
be set by the user:
 - `def` that places a variable on the environment
 - `echo` that echoes something back
 - `clear` that cleans the environment (needs to place all functions back in the environment)

Example:

```
> def ssid:meuwifi pwd:"12345678"
> wifi-connect $ssid $pwd
connected.
> echo $ssid
"meuwifi"
> clear
> echo $ssid
error: <error-code>
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

## Memory management

Arguments are parsed and inserted into an arena allocator,
they are copied by value and the arena is freed once the
command finishes execution.

Any insertion on the environment map results in a copy of the argument
to the map internal memory. The map is managed by an arena allocator
and memory is only freed all at once, that is, you can insert
items one by one, but only remove all of them at the same time.

This means no garbage collection is necessary.

## Eval

Here's what happens with a command, suppose we type:

```
wifi-connect meuwifi pwd:"12345678"
```

First it is tokenized by simply marking the starts and ends of
each lexeme, no copies happen here.
  
```
"wifi-connect", "meuwifi", "pwd", ":", "\"12345678\""
```

At the parsing stage, an argument list is built using an arena.
Simple arguments like numbers are directly stored in this list,
while strings that are dynamic in length
are created by referencing the source.

It is fine to reference the source at this stage, since we require
that the string representing a command lives for as long as the command is being
evaluated.

Anything that needs to live longer than the command execution needs to be copied.
This is the case for all strings used as keys or values in the environment.

The command is transformed into a linked list of arguments,
the first argument is interpreted as if it started with `$`
and the identifier is replaced by whatever value exists in
the environment for that key. This identifier is expected to be
command procedure. The previous command becomes:

```
<procedure>, <string>, <pair: (string, string)>
```

This procedure will be called and will receive
both the shell and the linked list of arguments, where the first
argument is itself (the procedure). 
