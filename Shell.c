/*
 * NAME: Kabhilesh Giri
 * NEU MAIL : giri.k@northeastern.edu
 * A simple Unix shell implementation in C.
 * Features:
 *   - Execution of commands with arguments
 *   - Input redirection (with '<') and output redirection (with '>')
 *   - Pipelining of multiple commands with '|'
 *   - Background execution with '&'
 *   - Built-in commands: "cd" to change directory, "exit" to exit the shell
 * The code follows best practices in indentation, naming, modularity, memory management, and error handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>

#define MAX_ARGS 128          // Maximum number of arguments for a command
#define MAX_PIPE 16           // Maximum number of pipeline segments in a command line

// Structure to represent a parsed command or a pipeline segment
typedef struct {
    char *args[MAX_ARGS];    // Arguments for the command (NULL-terminated list)
    char *input_file;        // Input redirection file (NULL if none)
    char *output_file;       // Output redirection file (NULL if none)
    bool background;         // True if command should run in the background
} Command;

/**
 * trim_whitespace - Remove leading and trailing whitespace from a string.
 * @str: The string to trim (modified in place).
 * Return: Pointer to the trimmed string (which may be str itself moved forward).
 */
char *trim_whitespace(char *str) {
    if (str == NULL) {
        return NULL;
    }
    // Skip leading whitespace
    while (isspace((unsigned char)*str)) {
        str++;
    }
    // If string is all whitespace, return an empty string
    if (*str == '\0') {
        return str;
    }
    // Remove trailing whitespace by placing a null terminator
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

/**
 * parse_command - Parse an input command line into one or more Command structures.
 * @input: The command line string (will be modified during parsing).
 * @commands: Array of Command structures to populate with parsed data.
 * @num_commands: Output parameter for number of commands (pipeline segments) parsed.
 * Return: 0 on successful parse, -1 on syntax error.
 *
 * The input is split into separate commands by the '|' pipe character. Each command segment
 * is further tokenized into arguments and checked for I/O redirection tokens ('<' or '>').
 * The background operator '&' is recognized if it appears at the end of the command line.
 * This function prints error messages to stderr for any syntactic errors (e.g., missing command
 * name, missing file for redirection, or misplacement of operators) and returns -1 in such cases.
 * On success, it fills the commands array and sets *num_commands.
 */
int parse_command(char *input, Command *commands, int *num_commands) {
    if (input == NULL) {
        return -1;
    }
    char *line_ptr = input;
    char *segment;
    char *segments[MAX_PIPE];
    int segment_count = 0;

    // Split the input line into pipeline segments using '|' as a delimiter
    while ((segment = strsep(&line_ptr, "|")) != NULL) {
        if (*segment == '\0') {
            // Empty segment (e.g., "||" or "|" at beginning/end)
            fprintf(stderr, "shell: syntax error near unexpected token '|'\n");
            return -1;
        }
        if (segment_count >= MAX_PIPE) {
            fprintf(stderr, "shell: too many pipeline segments (max %d)\n", MAX_PIPE);
            return -1;
        }
        segments[segment_count++] = segment;
    }

    // Parse each pipeline segment
    for (int i = 0; i < segment_count; ++i) {
        // Trim whitespace around the segment
        segments[i] = trim_whitespace(segments[i]);
        if (*segments[i] == '\0') {
            // Segment has no command (only whitespace)
            if (segment_count > 1) {
                fprintf(stderr, "missing command in pipeline\n");
            } else {
                fprintf(stderr, "missing command\n");
            }
            return -1;
        }
        // Initialize the Command structure
        Command *cmd = &commands[i];
        cmd->input_file = NULL;
        cmd->output_file = NULL;
        cmd->background = false;
        for (int j = 0; j < MAX_ARGS; ++j) {
            cmd->args[j] = NULL;
        }

        // Tokenize the segment into arguments and redirection tokens
        char *token_ptr = segments[i];
        char *token;
        int arg_index = 0;
        int input_count = 0;
        int output_count = 0;
        while ((token = strsep(&token_ptr, " \t")) != NULL) {
            if (*token == '\0') {
                // Skip empty tokens (caused by consecutive spaces)
                continue;
            }
            if (strcmp(token, "<") == 0) {
                // Input redirection
                token = strsep(&token_ptr, " \t");
                while (token && *token == '\0') {
                    token = strsep(&token_ptr, " \t");
                }
                if (token == NULL) {
                    fprintf(stderr, "syntax error near unexpected token '<'\n");
                    return -1;
                }
                if (input_count++ > 0) {
                    fprintf(stderr, "cannot redirect input more than once\n");
                    return -1;
                }
                cmd->input_file = token;
                continue;
            }
            if (strcmp(token, ">") == 0) {
                // Output redirection
                token = strsep(&token_ptr, " \t");
                while (token && *token == '\0') {
                    token = strsep(&token_ptr, " \t");
                }
                if (token == NULL) {
                    fprintf(stderr, "syntax error near unexpected token '>'\n");
                    return -1;
                }
                if (output_count++ > 0) {
                    fprintf(stderr, "cannot redirect output more than once\n");
                    return -1;
                }
                cmd->output_file = token;
                continue;
            }
            if (strcmp(token, "&") == 0) {
                // Background operator (must be at end of command line)
                if (i != segment_count - 1) {
                    fprintf(stderr, "'&' can only appear at end of command\n");
                    return -1;
                }
                // Ensure no other token follows '&'
                char *remain = token_ptr;
                bool only_spaces = true;
                while (remain && *remain != '\0') {
                    if (!isspace((unsigned char)*remain)) {
                        only_spaces = false;
                        break;
                    }
                    remain++;
                }
                if (!only_spaces) {
                    fprintf(stderr, "syntax error near unexpected token '&'\n");
                    return -1;
                }
                cmd->background = true;
                break;  // stop processing further tokens in this segment
            }
            // Normal argument token
            if (arg_index < MAX_ARGS - 1) {
                cmd->args[arg_index++] = token;
            } else {
                fprintf(stderr, "too many arguments (max %d)\n", MAX_ARGS - 1);
                return -1;
            }
        }
        cmd->args[arg_index] = NULL;
        if (arg_index == 0) {
            // No command found in this segment (only redirections or '&')
            fprintf(stderr, "missing command\n");
            return -1;
        }
    }

    // Validate pipeline redirection rules
    if (segment_count > 1) {
        for (int i = 0; i < segment_count; ++i) {
            if (i != 0 && commands[i].input_file != NULL) {
                fprintf(stderr, "input redirection not allowed for command %d in pipeline\n", i + 1);
                return -1;
            }
            if (i != segment_count - 1 && commands[i].output_file != NULL) {
                fprintf(stderr, "output redirection not allowed for command %d in pipeline\n", i + 1);
                return -1;
            }
        }
    }

    *num_commands = segment_count;
    return 0;
}

/**
 * redirect_io - Configure input/output redirection for a command in the child process.
 * @cmd: The Command structure containing redirection info.
 * Return: 0 on success, -1 if an error occurs (and an error message is printed).
 */
int redirect_io(const Command *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s : File not found\n", cmd->input_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "error duplicating file descriptor for input: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }
    if (cmd->output_file) {
        int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "%s: Cannot create file\n", cmd->output_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "error duplicating file descriptor for output: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

/**
 * execute_commands - Execute the parsed command(s).
 * @commands: Array of Command structures to execute.
 * @num_commands: Number of commands (segments) in the array.
 * Return: 1 to continue the shell loop, or 2 to signal that the shell should exit.
 *
 * If a single command is a built-in (cd or exit), it is handled in the shell process.
 * Otherwise, external commands are executed by forking child processes. If multiple commands
 * are present (pipeline), pipes are set up between them. The function handles foreground and
 * background execution: for background jobs, it does not wait for the child process to finish.
 */
int execute_commands(Command *commands, int num_commands) {
    if (num_commands <= 0) {
        return 1;  // nothing to execute
    }
    // Handle built-in commands for a single command (no pipeline)
    if (num_commands == 1) {
        Command *cmd = &commands[0];
        if (cmd->args[0] == NULL) {
            return 1;
        }
        if (strcmp(cmd->args[0], "exit") == 0) {
            return 2;  // signal to exit the shell
        }
        if (strcmp(cmd->args[0], "cd") == 0) {
            const char *dir = cmd->args[1];
            if (dir == NULL) {
                // Change to HOME directory if no argument
                dir = getenv("HOME");
                if (dir == NULL) {
                    dir = ".";
                }
            }
            if (chdir(dir) != 0) {
                fprintf(stderr, "cd: %s: %s\n", cmd->args[1] ? cmd->args[1] : dir, strerror(errno));
            }
            return 1;
        }
    }

    // Execute external command(s), possibly with pipes
    pid_t pids[MAX_PIPE];
    int prev_fd = -1;
    int launched = 0;

    for (int i = 0; i < num_commands; ++i) {
        int pipefd[2];
        if (i < num_commands - 1) {
            // Create a pipe for this and the next command
            if (pipe(pipefd) < 0) {
                perror("shell: pipe");
                break;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("shell: fork");
            if (i < num_commands - 1) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            break;
        }
        if (pid == 0) {
            // Child process
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
            }
            if (i < num_commands - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
            }
            // Close unused fds in child
            if (prev_fd != -1) {
                close(prev_fd);
            }
            if (i < num_commands - 1) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            if (redirect_io(&commands[i]) != 0) {
                _exit(1);
            }
            execvp(commands[i].args[0], commands[i].args);
            // If execvp returns, an error occurred
            fprintf(stderr, "%s: Command not found\n", commands[i].args[0]);
            _exit(127);
        } else {
            // Parent process
            pids[launched++] = pid;
            if (prev_fd != -1) {
                close(prev_fd);
            }
            if (i < num_commands - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }
    }
    // Close any remaining pipe read end in parent
    if (prev_fd != -1) {
        close(prev_fd);
    }

    // If last command is to run in background, do not wait for children
    if (commands[num_commands - 1].background) {
        printf("[%d]\n", pids[launched - 1]);
        fflush(stdout);
    } else {
        // Wait for all child processes in the pipeline
        for (int j = 0; j < launched; ++j) {
            waitpid(pids[j], NULL, 0);
        }
    }
    return 1;
}

/**
 * main - Entry point of the shell program.
 * Return: 0 on normal exit, or a non-zero value on error.
 *
 * The main loop reads input lines, parses them, and executes the resulting command(s).
 * It prints a prompt in interactive mode and handles EOF (Ctrl-D) to exit. The shell
 */
int main(void) {
    char *input_line = NULL;
    size_t bufsize = 0;
    Command commands[MAX_PIPE];
    int num_commands;
    int status = 1;

    // Shell read-execute loop
    while (1) {
        // Print prompt if interactive
        if (isatty(STDIN_FILENO)) {
            printf("$ ");
            fflush(stdout);
        }
        // Read a line of input (getline allocates buffer as needed)
        ssize_t nread = getline(&input_line, &bufsize, stdin);
        if (nread == -1) {
            // Exit on EOF or read error
            if (isatty(STDIN_FILENO)) {
                printf("\n");
            }
            break;
        }
        // Remove the trailing newline, if any
        if (nread > 0 && input_line[nread - 1] == '\n') {
            input_line[nread - 1] = '\0';
        }
        // Skip empty lines
        char *trimmed = trim_whitespace(input_line);
        if (*trimmed == '\0') {
            continue;
        }
        // Parse the command line
        if (parse_command(trimmed, commands, &num_commands) != 0) {
            // Parsing error (message already printed)
            continue;
        }
        // Execute the parsed command(s)
        status = execute_commands(commands, num_commands);
        if (status == 2) {
            // "exit" command: break out of loop
            break;
        }
        // Reap any background processes that have finished
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            continue;
        }
    }

    free(input_line);
    return 0;
}
