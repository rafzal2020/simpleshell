#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#define BUF_SIZE 1024
#define MAX_TOKENS 64

// search dirs for bare command names
const char *search_dirs[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};

// find full path of a bare command name
int find_path(char *name, char *result)
{
    for (int i = 0; search_dirs[i] != NULL; i++)
    {
        snprintf(result, BUF_SIZE, "%s/%s", search_dirs[i], name);
        if (access(result, X_OK) == 0)
            return 1;
    }
    return 0;
}

// remove everything after # character
void strip_comment(char *buf)
{
    char *comment = strchr(buf, '#');
    if (comment != NULL)
    {
        *comment = '\0';
    }
}

// separate command line by whitespace
int tokenize_input(char *buf, char *tokens[])
{
    int num_tokens = 0;

    // strtok modifies buf by inserting '\0' terminators
    char *token = strtok(buf, " \t");
    while (token != NULL && num_tokens < MAX_TOKENS - 1)
    {
        tokens[num_tokens++] = token;
        token = strtok(NULL, " \t");
    }

    // argument array ends with NULL
    tokens[num_tokens] = NULL;
    return num_tokens;
}

// parse token list
// separate <, > from actual command arguments
int parse_redirection(char *tokens[], int num_tokens,
                      char *args_cleaned[],
                      char **in_file, char **out_file)
{
    int num_args = 0;

    // default = no redirection
    *in_file = NULL;
    *out_file = NULL;

    for (int i = 0; i < num_tokens; i++)
    {
        // input redirection
        if (strcmp(tokens[i], "<") == 0)
        {
            // error if no filename after <
            if (i + 1 >= num_tokens)
                return -1;

            // store input file, skip over it
            *in_file = tokens[++i];
        }

        // same for output redirection
        else if (strcmp(tokens[i], ">") == 0)
        {
            if (i + 1 >= num_tokens)
                return -1;

            *out_file = tokens[++i];
        }
        else
        {
            // add actual arguments to args_cleaned
            args_cleaned[num_args++] = tokens[i];
        }
    }

    // make sure args_cleaned ends with NULL
    args_cleaned[num_args] = NULL;

    // error if there are no actual commands
    if (num_args == 0)
        return -1;

    return num_args;
}

int matches_pattern(const char *name, const char *prefix, const char *suffix)
{
    size_t name_len = strlen(name);
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);

    // filename must be long enough to contain both prefix and suffix
    if (name_len < prefix_len + suffix_len)
        return 0;

    // must start with prefix
    if (strncmp(name, prefix, prefix_len) != 0)
        return 0;

    // must end with suffix
    if (strcmp(name + name_len - suffix_len, suffix) != 0)
        return 0;

    return 1;
}

int expand_wildcard_token(char *token, char *expanded_args[], int allocated_flags[], int num_expanded)
{
    char token_copy[BUF_SIZE];
    char dir_path[BUF_SIZE];
    char pattern[BUF_SIZE];
    char prefix[BUF_SIZE];
    char suffix[BUF_SIZE];
    char *slash;
    char *star;
    DIR *dir;
    struct dirent *entry;
    int found_match = 0;

    strncpy(token_copy, token, BUF_SIZE - 1);
    token_copy[BUF_SIZE - 1] = '\0';

    // split into directory path and last filename section
    slash = strrchr(token_copy, '/');
    if (slash != NULL)
    {
        *slash = '\0';
        strncpy(dir_path, token_copy, BUF_SIZE - 1);
        dir_path[BUF_SIZE - 1] = '\0';

        strncpy(pattern, slash + 1, BUF_SIZE - 1);
        pattern[BUF_SIZE - 1] = '\0';
    }
    else
    {
        strcpy(dir_path, ".");
        strncpy(pattern, token_copy, BUF_SIZE - 1);
        pattern[BUF_SIZE - 1] = '\0';
    }

    // find the single '*'
    star = strchr(pattern, '*');
    if (star == NULL)
    {
        expanded_args[num_expanded++] = token;
        return num_expanded;
    }

    // split pattern into prefix and suffix
    *star = '\0';
    strncpy(prefix, pattern, BUF_SIZE - 1);
    prefix[BUF_SIZE - 1] = '\0';

    strncpy(suffix, star + 1, BUF_SIZE - 1);
    suffix[BUF_SIZE - 1] = '\0';

    dir = opendir(dir_path);
    if (dir == NULL)
    {
        // if directory can't be opened, keep token unchanged
        expanded_args[num_expanded] = token;
        allocated_flags[num_expanded] = 0;
        return num_expanded + 1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char *name = entry->d_name;

        // hidden file rule:
        // patterns like *.txt (empty prefix) do not match names starting with '.'
        if (prefix[0] == '\0' && name[0] == '.')
            continue;

        if (matches_pattern(name, prefix, suffix))
        {
            found_match = 1;

            if (strcmp(dir_path, ".") == 0)
            {
                // no directory prefix in original token
                expanded_args[num_expanded] = malloc(strlen(name) + 1);
                if (expanded_args[num_expanded] == NULL)
                {
                    closedir(dir);
                    return num_expanded;
                }
                strcpy(expanded_args[num_expanded], name);
            }
            else
            {
                // preserve directory prefix in expanded result
                size_t len = strlen(dir_path) + 1 + strlen(name) + 1;
                expanded_args[num_expanded] = malloc(len);
                if (expanded_args[num_expanded] == NULL)
                {
                    closedir(dir);
                    return num_expanded;
                }
                snprintf(expanded_args[num_expanded], len, "%s/%s", dir_path, name);
            }

            allocated_flags[num_expanded] = 1;
            num_expanded++;
        }
    }

    closedir(dir);

    // if nothing matched, keep original token unchanged
    if (!found_match)
    {
        expanded_args[num_expanded] = token;
        allocated_flags[num_expanded] = 0;
        num_expanded++;
    }

    return num_expanded;
}

int expand_wildcards(char *args_cleaned[], int num_args, char *expanded_args[], int allocated_flags[])
{
    int num_expanded = 0;

    for (int i = 0; i < num_args; i++)
    {
        if (strchr(args_cleaned[i], '*') != NULL)
        {
            num_expanded = expand_wildcard_token(args_cleaned[i],
                                                 expanded_args,
                                                 allocated_flags,
                                                 num_expanded);
        }
        else
        {
            expanded_args[num_expanded] = args_cleaned[i];
            allocated_flags[num_expanded] = 0;
            num_expanded++;
        }
    }

    expanded_args[num_expanded] = NULL;
    return num_expanded;
}

void free_expanded_args(char *expanded_args[], int allocated_flags[], int num_expanded)
{
    for (int i = 0; i < num_expanded; i++)
    {
        if (allocated_flags[i])
            free(expanded_args[i]);
    }
}

// find executable path
// if command contains /, treat it as a path
// otherwise use find_path
int resolve_command_path(char *cmd, char *path)
{
    if (strchr(cmd, '/') != NULL)
    {
        strncpy(path, cmd, BUF_SIZE - 1);
        path[BUF_SIZE - 1] = '\0';
        return 1;
    }

    return find_path(cmd, path);
}

// in child process, apply requested redirection
int apply_child_redirection(char *in, char *out)
{
    if (in != NULL)
    {
        int fd = open(in, O_RDONLY);
        if (fd < 0)
        {
            perror(in);
            return 0;
        }

        // replace standard input (fd 0) with this file
        if (dup2(fd, STDIN_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return 0;
        }

        close(fd);
    }

    if (out != NULL)
    {
        int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (fd < 0)
        {
            perror(out);
            return 0;
        }

        // replace standard output (fd 1) with this file
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return 0;
        }

        close(fd);
    }

    return 1;
}

// report if child failed in interactive mode
void report_child_status(int status, int interactive)
{
    if (!interactive)
        return;

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0)
        {
            char msg[BUF_SIZE];
            int len = snprintf(msg, sizeof(msg),
                               "Exited with status %d\n", exit_code);
            write(STDOUT_FILENO, msg, len);
        }
    }
    else if (WIFSIGNALED(status))
    {
        char msg[BUF_SIZE];
        int sig = WTERMSIG(status);
        int len = snprintf(msg, sizeof(msg),
                           "Terminated by signal %d\n", sig);
        write(STDOUT_FILENO, msg, len);
    }
}

// run external command
int external_command(char *args_cleaned[], char *in, char *out, int interactive)
{
    char path[BUF_SIZE];

    // find executable file to be run
    if (!resolve_command_path(args_cleaned[0], path))
    {
        write(STDERR_FILENO, "Command not found\n", 18);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 0;
    }

    // child process
    if (pid == 0)
    {
        // set up requested redirection
        if (!apply_child_redirection(in, out))
            exit(EXIT_FAILURE);

        // replace child process with target program
        execv(path, args_cleaned);

        // only reached if execv fails
        perror("execv");
        exit(EXIT_FAILURE);
    }
    else
    // parent process
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            perror("waitpid");
            return 0;
        }

        report_child_status(status, interactive);
    }

        return 0;
}

void execute_pipeline(char *tokens[], int num_tokens, int interactive)
{
    int cmd_starts[MAX_TOKENS];
    int cmd_lengths[MAX_TOKENS];
    int num_cmds = 0;
    int start = 0;

    // split tokens on '|'
    for (int i = 0; i <= num_tokens; i++)
    {
        if (i == num_tokens || strcmp(tokens[i], "|") == 0)
        {
            cmd_starts[num_cmds] = start;
            cmd_lengths[num_cmds] = i - start;
            num_cmds++;
            start = i + 1;
        }
    }

    // check for exit in pipeline per spec ("foo | exit" should terminate)
    for (int i = 0; i < num_cmds; i++)
    {
        int s = cmd_starts[i];
        if (cmd_lengths[i] > 0 && strcmp(tokens[s], "exit") == 0)
        {
            if (interactive)
                write(STDOUT_FILENO, "Exiting my shell.\n", 18);
            exit(EXIT_SUCCESS);
        }
    }

    // validate — no empty subcommands (e.g. "ls |" or "| grep foo")
    for (int i = 0; i < num_cmds; i++)
    {
        if (cmd_lengths[i] == 0)
        {
            write(STDERR_FILENO, "Syntax error\n", 13);
            return;
        }
    }

    // create all pipes upfront
    int pipes[MAX_TOKENS][2];
    for (int i = 0; i < num_cmds - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            return;
        }
    }

    pid_t pids[MAX_TOKENS];

    for (int i = 0; i < num_cmds; i++)
    {
        char *args[MAX_TOKENS];
        int len = cmd_lengths[i];
        int s = cmd_starts[i];

        for (int j = 0; j < len; j++)
            args[j] = tokens[s + j];
        args[len] = NULL;

        // resolve path
        char path[BUF_SIZE];
        if (!resolve_command_path(args[0], path))
        {
            write(STDERR_FILENO, "Command not found\n", 18);
            // close all pipes before returning
            for (int j = 0; j < num_cmds - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }

        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("fork");
            return;
        }

        if (pids[i] == 0)
        {
            // if not first: read from previous pipe
            if (i > 0)
                dup2(pipes[i - 1][0], STDIN_FILENO);

            // if not last: write to next pipe
            if (i < num_cmds - 1)
                dup2(pipes[i][1], STDOUT_FILENO);

            // child closes all pipe fds after dup2
            for (int j = 0; j < num_cmds - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execv(path, args);
            perror("execv");
            exit(EXIT_FAILURE);
        }
    }

    // parent MUST close all pipe ends
    // otherwise last process hangs waiting for EOF that never comes
    for (int i = 0; i < num_cmds - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // wait for all children
    int status = 0;
    for (int i = 0; i < num_cmds; i++)
        waitpid(pids[i], &status, 0);

    // spec says: pipeline succeeds if and only if LAST command succeeds
    report_child_status(status, interactive);
}

// main execution function
int execute_command(char *buf, int interactive)
{

    // run through parsing helper functions
    strip_comment(buf);

    if (buf[0] == '\0')
        return 0;

    char *tokens[MAX_TOKENS];
    int num_tokens = tokenize_input(buf, tokens);

    if (num_tokens == 0)
        return 0;

    int has_pipe = 0;
    for (int i = 0; i < num_tokens; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            has_pipe = 1;
            break;
        }
    }

    // hand off to pipeline executor
    if (has_pipe)
    {
        execute_pipeline(tokens, num_tokens, interactive);
        return 0;
    }

    char *args_cleaned[MAX_TOKENS];
    char *in = NULL;
    char *out = NULL;
    int num_args = parse_redirection(tokens, num_tokens,
                                     args_cleaned, &in, &out);

    if (num_args < 0)
    {
        write(STDERR_FILENO, "Syntax error\n", 13);
        return 0;
    }

    char *expanded_args[MAX_TOKENS];
    int allocated_flags[MAX_TOKENS];
    int num_expanded = expand_wildcards(args_cleaned, num_args, expanded_args, allocated_flags);

    // built-in: exit
    if (strcmp(expanded_args[0], "exit") == 0)
    {
        free_expanded_args(expanded_args, allocated_flags, num_expanded);
        return 1;
    }

    // built-in: cd
    else if (strcmp(expanded_args[0], "cd") == 0)
    {
        char *dir;

        // cd with no args goes to HOME
        if (num_expanded == 1)
            dir = getenv("HOME");

        // otherwise transfer to given directory
        else if (num_expanded == 2)
            dir = expanded_args[1];
        else
        {
            write(STDOUT_FILENO, "cd: too many arguments\n", 23);
            free_expanded_args(expanded_args, allocated_flags, num_expanded);
            return 0;
        }

        if (dir == NULL)
        {
            write(STDERR_FILENO, "cd: HOME not set\n", 17);
            free_expanded_args(expanded_args, allocated_flags, num_expanded);
            return 0;
        }

        if (chdir(dir) != 0)
        {
            perror("cd");
        }
        free_expanded_args(expanded_args, allocated_flags, num_expanded);
        return 0;
    }

    // built-in: pwd
    else if (strcmp(expanded_args[0], "pwd") == 0)
    {
        int saved_stdout = -1;
        int fd = -1;

        // if output redirection was requested
        // temporarily redirect stdout
        if (out != NULL)
        {
            fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd < 0)
            {
                perror(out);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            // save original stdout for later
            saved_stdout = dup(STDOUT_FILENO);
            if (saved_stdout < 0)
            {
                perror("dup");
                close(fd);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            // send stdout to output file
            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                perror("dup2");
                close(fd);
                close(saved_stdout);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            close(fd);
        }

        // print current working directory
        char cwd[BUF_SIZE];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("pwd");
        }
        else
        {
            write(STDOUT_FILENO, cwd, strlen(cwd));
            write(STDOUT_FILENO, "\n", 1);
        }

        // restore normal stdout
        if (saved_stdout != -1)
        {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        free_expanded_args(expanded_args, allocated_flags, num_expanded);
        return 0;
    }

    // built-in: which
    else if (strcmp(expanded_args[0], "which") == 0)
    {
        int saved_stdout = -1;
        int fd = -1;
        char path[BUF_SIZE];

        // if output redirection was requested
        // temporarily redirect stdout
        if (out != NULL)
        {
            fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd < 0)
            {
                perror(out);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            saved_stdout = dup(STDOUT_FILENO);
            if (saved_stdout < 0)
            {
                perror("dup");
                close(fd);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                perror("dup2");
                close(fd);
                close(saved_stdout);
                free_expanded_args(expanded_args, allocated_flags, num_expanded);
                return 0;
            }

            close(fd);
        }

        // which takes exactly 1 argument
        if (num_expanded != 2)
        {
            // fail
        }
        else if (strcmp(expanded_args[1], "cd") == 0 ||
                 strcmp(expanded_args[1], "pwd") == 0 ||
                 strcmp(expanded_args[1], "which") == 0 ||
                 strcmp(expanded_args[1], "exit") == 0)
        {
            // built-ins do nothing with which
        }

        // treat argument as path with /
        else if (strchr(expanded_args[1], '/') != NULL)
        {
            if (access(expanded_args[1], X_OK) == 0)
            {
                write(STDOUT_FILENO, expanded_args[1], strlen(expanded_args[1]));
                write(STDOUT_FILENO, "\n", 1);
            }
        }

        // otherwise search directories
        else if (find_path(expanded_args[1], path))
        {
            write(STDOUT_FILENO, path, strlen(path));
            write(STDOUT_FILENO, "\n", 1);
        }

        // restore stdout if it was redirected
        if (saved_stdout != -1)
        {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }

        free_expanded_args(expanded_args, allocated_flags, num_expanded);

        return 0;
    }

    // external command: fork + execv
    int result = external_command(expanded_args, in, out, interactive);
    free_expanded_args(expanded_args, allocated_flags, num_expanded);
    return result;
}

void input_loop(int input_fd, int interactive)
{
    char buf[BUF_SIZE];
    char cwd[BUF_SIZE];          // working directory
    char *home = getenv("HOME"); // '~'
    int pos;
    char c;

    while (1)
    {
        getcwd(cwd, sizeof(cwd));

        if (interactive)
        {

            if (home != NULL && strncmp(cwd, home, strlen(home)) == 0)
            {
                write(STDOUT_FILENO, "~", 1);
                write(STDOUT_FILENO, cwd + strlen(home), strlen(cwd + strlen(home)));
            }
            else
            {
                write(STDOUT_FILENO, cwd, strlen(cwd));
            }
            write(STDOUT_FILENO, "$ ", 2);
        }

        pos = 0;
        while (1)
        {
            int n = read(input_fd, &c, 1);

            if (n < 0)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }

            if (n == 0)
            {
                if (interactive)
                    write(STDOUT_FILENO, "Exiting my shell.\n", 18);
                return;
            }

            if (c == '\n')
            {
                buf[pos] = '\0';
                break;
            }

            if (pos < BUF_SIZE - 1)
                buf[pos++] = c;
        }

        // executing commands
        if (buf[0] == '\0' || buf[0] == '#')
        {
            continue; // empty line or comment
        }

        int should_exit = execute_command(buf, interactive);
        if (should_exit)
        {
            if (interactive)
                write(STDOUT_FILENO, "Exiting my shell.\n", 18);
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    int interactive;
    int input_fd;

    if (argc == 1)
    {
        input_fd = STDIN_FILENO;
        interactive = isatty(STDIN_FILENO);
    }
    else if (argc == 2)
    {
        input_fd = open(argv[1], O_RDONLY);
        if (input_fd < 0)
        {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
        interactive = 0;
    }
    else
    {
        write(STDERR_FILENO, "Usage: ./mysh [file]\n", 21);
        return EXIT_FAILURE;
    }

    if (interactive)
        write(STDOUT_FILENO, "Welcome to my shell.\n", 21);

    input_loop(input_fd, interactive);

    if (input_fd != STDIN_FILENO)
        close(input_fd);

    return EXIT_SUCCESS;
}