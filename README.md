# CS 214 — Project III: mysh
## README & Test Plan

Rayaan Afzal / ra965

Anthony Rahner / arr234

---

## Project Overview

mysh is a simple Unix command-line shell implemented in C. It supports interactive and batch modes, built-in commands (`cd`, `pwd`, `which`, `exit`), external program execution via `fork`/`execv`, I/O redirection with `<` and `>`, pipelines using `|`, wildcard/glob expansion with `*`, and correct prompt formatting with `~` substitution.

---

## Build Instructions

```
make          # builds the mysh executable
make clean    # removes object files and binary
```

---

## Test Plan

Our testing strategy is organized into categories that mirror the spec requirements. Each category contains manual test cases verified by running mysh directly and comparing output against expected behavior. Test input files are provided for batch mode scenarios.

---

### 1. Interactive Mode & Prompt

Goal: Verify welcome/goodbye messages and prompt formatting.

**Test 1.1 — Welcome and prompt on startup:**
```
$ ./mysh
Welcome to my shell.
~$
```
Expected: Welcome message prints, prompt shows `~` when in home directory.

**Test 1.2 — Prompt shows working directory:**
```
~$ cd Documents
~/Documents$
```
Expected: Prompt updates to reflect new directory with `~` prefix.

**Test 1.3 — Prompt outside home directory:**
```
~$ cd /tmp
/tmp$
```
Expected: Full path shown, no `~` prefix when outside home.

**Test 1.4 — Goodbye on Ctrl+D (EOF):**
```
~$ [Ctrl+D]
Exiting my shell.
```
Expected: Shell exits cleanly with goodbye message.

**Test 1.5 — Goodbye on exit command:**
```
~$ exit
Exiting my shell.
```
Expected: Same clean exit as EOF.

---

### 2. Batch Mode

Goal: Verify no prompts or messages are printed in batch mode.

**Test 2.1 — Batch mode from file argument:**
```
$ ./mysh test_basic.sh
```
Expected: Commands execute silently, no prompt or welcome/goodbye printed.

**Test 2.2 — Batch mode via stdin pipe:**
```
$ cat test_basic.sh | ./mysh
```
Expected: Same output as file argument, no prompts.

**Test 2.3 — /dev/null as default stdin in batch mode:**
```
$ echo 'cat' | ./mysh
```
Expected: `cat` receives `/dev/null` as input and outputs nothing (does not block).

---

### 3. Built-in Commands

Goal: Verify `cd`, `pwd`, `which`, and `exit` behave per spec.

**Test 3.1 — cd with no arguments goes home:**
```
~/Documents$ cd
~$
```

**Test 3.2 — cd with relative path:**
```
~$ cd Documents/CS214
~/Documents/CS214$
```

**Test 3.3 — cd .. moves up one directory:**
```
~/Documents/CS214$ cd ..
~/Documents$
```

**Test 3.4 — cd with nonexistent directory prints error:**
```
~$ cd doesnotexist
cd: No such file or directory
```
Expected: Error printed, shell continues, directory unchanged.

**Test 3.5 — cd with too many arguments fails:**
```
~$ cd foo bar
cd: too many arguments
```

**Test 3.6 — pwd prints current directory:**
```
~/Documents$ pwd
/home/user/Documents
```

**Test 3.7 — which finds a program:**
```
~$ which ls
/bin/ls
```

**Test 3.8 — which on a built-in prints nothing and fails:**
```
~$ which cd
```
Expected: No output, command fails silently.

**Test 3.9 — which on unknown program prints nothing:**
```
~$ which doesnotexist
```
Expected: No output.

---

### 4. External Command Execution

Goal: Verify programs are found and executed correctly via `fork`/`execv`.

**Test 4.1 — Simple external command:**
```
~$ echo hello
hello
```

**Test 4.2 — Command with arguments:**
```
~$ ls -l /tmp
```
Expected: Long listing of `/tmp` printed.

**Test 4.3 — Command with absolute path:**
```
~$ /bin/echo hello
hello
```

**Test 4.4 — Unknown command prints error:**
```
~$ notarealcommand
Command not found
```
Expected: Shell continues after error.

**Test 4.5 — Exit status reported for non-zero exit:**
```
~$ ls /doesnotexist
Exited with status 2
```
Expected: In interactive mode, non-zero exit code is reported.

---

### 5. I/O Redirection

Goal: Verify `<` and `>` redirect stdin/stdout correctly.

**Test 5.1 — Output redirection creates file:**
```
~$ echo hello > out.txt
~$ cat out.txt
hello
```

**Test 5.2 — Output redirection truncates existing file:**
```
~$ echo first > out.txt
~$ echo second > out.txt
~$ cat out.txt
second
```
Expected: File contains only `second`.

**Test 5.3 — Input redirection reads from file:**
```
~$ cat < out.txt
second
```

**Test 5.4 — Both redirections together:**
```
~$ cat < out.txt > copy.txt
~$ cat copy.txt
second
```

**Test 5.5 — Redirections in either order (spec requirement):**
```
~$ cat > quux < baz
```
Expected: Behaves identically to `cat < baz > quux`.

**Test 5.6 — Redirect to unwritable location prints error:**
```
~$ echo hi > /root/nope.txt
```
Expected: Permission error printed, command fails, shell continues.

**Test 5.7 — pwd output redirected to file:**
```
~$ pwd > dir.txt
~$ cat dir.txt
```
Expected: Current directory path written to `dir.txt`.

---

### 6. Pipelines

Goal: Verify `|` correctly connects stdout of one process to stdin of the next.

**Test 6.1 — Simple two-process pipeline:**
```
~$ echo hello | cat
hello
```

**Test 6.2 — Pipeline with grep:**
```
~$ echo -e 'foo\nbar\nbaz' | grep ba
bar
baz
```

**Test 6.3 — Three-process pipeline:**
```
~$ ls /bin | sort | head -5
```
Expected: First 5 sorted entries of `/bin` printed.

**Test 6.4 — Pipeline success based on last command:**
```
~$ ls /doesnotexist | cat
```
Expected: `cat` exits 0 so pipeline succeeds even though `ls` failed.

**Test 6.5 — foo | exit terminates shell:**
```
~$ echo hi | exit
Exiting my shell.
```
Expected: Shell terminates once `echo` completes.

---

### 7. Wildcard Expansion

Goal: Verify `*` globs expand to matching filenames.

**Test 7.1 — Basic wildcard matches files:**
```
~$ echo *.c
```
Expected: Lists all `.c` files in working directory.

**Test 7.2 — No match passes token unchanged:**
```
~$ echo *.xyz
*.xyz
```
Expected: Token passed literally when no files match.

**Test 7.3 — Wildcard does not match hidden files:**
```
~$ echo *
```
Expected: Does not include files beginning with `.`.

**Test 7.4 — Wildcard in subdirectory path:**
```
~$ echo src/*.c
```
Expected: Expands to all `.c` files inside `src/`.

---

### 8. Comments

Goal: Verify `#` starts a comment and the rest of the line is ignored.

**Test 8.1 — Comment-only line does nothing:**
```
~$ # this is a comment
```
Expected: No output, prompt returns immediately.

**Test 8.2 — Inline comment truncates command:**
```
~$ echo hello # world
hello
```
Expected: Only `hello` printed; `# world` ignored.

---

### 9. Edge Cases & Error Handling

Goal: Verify robustness against malformed input and boundary conditions.

**Test 9.1 — Empty line does nothing:**
```
~$ [Enter]
```
Expected: Prompt returns, no error.

**Test 9.2 — Syntax error `< <` is handled:**
```
~$ cat < <
Syntax error
```
Expected: Error printed, shell continues.

**Test 9.3 — Trailing `|` is a syntax error:**
```
~$ ls |
Syntax error
```

**Test 9.4 — mysh exits EXIT_SUCCESS normally:**
```
$ echo exit | ./mysh; echo $?
0
```

**Test 9.5 — mysh exits EXIT_FAILURE when file not found:**
```
$ ./mysh doesnotexist.sh; echo $?
```
Expected: Error message and non-zero exit code.

**Test 9.6 — Signal termination reported:**

Run a program that is killed by SIGKILL. Expected: `Terminated by signal N` message in interactive mode.

---

## Test Input Files

The following `.sh` files are included as batch-mode test inputs. Run them with `./mysh <filename>`:

| File | What it tests |
|------|---------------|
| `test_basic.sh` | Basic commands: echo, ls, pwd, cd |
| `redirection.sh` | Input and output redirection scenarios |
| `pipeline.sh` | Single and multi-stage pipelines |
| `wildcards.sh` | Wildcard expansion cases |
| `errors.sh` | Syntax errors, bad paths, bad redirections |
| `builtins.sh` | All four built-in commands including edge cases |

---

## Notes

All test cases were run in both interactive mode (`./mysh`) and batch mode (`./mysh testfile.sh` or piped via `cat`). The shell was verified to always return `EXIT_SUCCESS` except when given a nonexistent script file.
