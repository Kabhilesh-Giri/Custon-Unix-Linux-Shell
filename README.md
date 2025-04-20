# Custon-Unix-Linux-Shell

**Author:** Kabhilesh Giri  
**Email:** giri.k@northeastern.edu  

A lightweight, UNIX-like command-line shell implemented in C, supporting core functionalities such as command execution, pipelining, redirection, background processes, and built-in commands like cd and exit.

## Features
### Command Execution
- Supports running standard UNIX commands with arguments.

### Pipelining (|)
- Implements multi-stage pipelines (cmd1 | cmd2 | cmd3) with inter-process communication using pipe() and dup2().

### Input/Output Redirection (<, >)
- Handles redirection of input and output streams to/from files.

### Background Execution (&)
- Runs commands asynchronously and returns control to the shell immediately, displaying the PID of the last sub-command in the pipeline.

### Built-in Commands

- cd: Changes the current working directory.

exit: Terminates the shell.

### Whitespace Handling
- Ignores leading/trailing/multiple whitespaces and handles malformed inputs gracefully.

### Error Handling
- Detects and reports syntax errors (e.g., missing command in pipeline, multiple redirections).

## How It Works
- **Parsing**: The shell reads a full input line, tokenizes it using delimiters (`|`, `<`, `>`, `&`), and builds a pipeline of `Command` structs.
- **Redirection Handling**: File descriptors are replaced using `dup2()` for any specified `<` or `>` redirection.
- **Execution**: Commands are executed in either foreground or background using `fork()`, `execvp()`, and `waitpid()`.
- **Pipelining**: `pipe()` and descriptor chaining allow command segments to pass output to one another.

---

## Usage 
- **make run**
- **make clean**

---

## Try commands
- **ls -l**
- **cat file.txt | grep keyword | sort > sorted.txt**
- **sleep 10 &**
- **cd /tmp**
- **exit**

## Files
- **Shell.c** — Source code of the shell
- **README.md** — Project documentation
- **makefile** — For compiling and cleaning

## Author Notes
This shell was developed as part of my systems programming journey at Northeastern University. It reinforces my experience with process control, file descriptors, system calls, and building minimal yet functional user-space tools.
