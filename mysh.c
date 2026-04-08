#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

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

int execute_command(char *buf, int interactive)
{
    (void)interactive; // to be used in the future

    if (buf[0] == '\0')
        return 0;

    // strip comment starting at first #
    char *comment = strchr(buf, '#');
    if (comment != NULL)
    {
        *comment = '\0';
    }

    // tokenize
    char *tokens[MAX_TOKENS];
    int num_tokens = 0;
    char *token = strtok(buf, " \t");
    while (token != NULL && num_tokens < MAX_TOKENS - 1)
    {
        tokens[num_tokens++] = token;
        token = strtok(NULL, " \t");
    }
    tokens[num_tokens] = NULL;

    if (num_tokens == 0)
        return 0;

    // built-in: exit
    if (strcmp(tokens[0], "exit") == 0)
    {
        return 1;
    }

    // built-in: cd
    else if (strcmp(tokens[0], "cd") == 0)
    {
        char *dir;
        if (num_tokens == 1)
            dir = getenv("HOME");
        else if (num_tokens == 2)
            dir = tokens[1];
        else
        {
            write(STDOUT_FILENO, "cd: too many arguments\n", 23);
            return 0;
        }

        if (dir == NULL)
        {
            write(STDERR_FILENO, "cd: HOME not set\n", 17);
            return 0;
        }

        if (chdir(dir) != 0)
        {
            perror("cd");
        }

        return 0;
    }

    // built-in: pwd
    else if (strcmp(tokens[0], "pwd") == 0)
    {
        char cwd[BUF_SIZE];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("pwd");
            return 0;
        }
        write(STDOUT_FILENO, cwd, strlen(cwd));
        write(STDOUT_FILENO, "\n", 1);
        return 0;
    }

    // external command: fork + execv
    else
    {
        char path[BUF_SIZE];

        // if it contains '/', use it directly, otherwise search
        if (strchr(tokens[0], '/') != NULL)
        {
            strncpy(path, tokens[0], BUF_SIZE);
            path[BUF_SIZE - 1] = '\0';
        }
        else if (!find_path(tokens[0], path))
        {
            write(STDOUT_FILENO, "Command not found\n", 18);
            return 0;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return 0;
        }

        if (pid == 0)
        {
            // child: execute the program
            execv(path, tokens);
            perror("execv");
            exit(EXIT_FAILURE);
        }
        else
        {
            // parent: wait for child to finish
            int status;
            if (waitpid(pid, &status, 0) < 0)
            {
                perror("waitpid");
            }
        }
    }
    return 0;
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