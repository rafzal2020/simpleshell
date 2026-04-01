#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define MAX_TOKENS 64

// search dirs for bare command names
const char *search_dirs[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};

// find full path of a bare command name
int find_path(char *name, char *result) {
    for (int i = 0; search_dirs[i] != NULL; i++) {
        snprintf(result, BUF_SIZE, "%s/%s", search_dirs[i], name);
        if (access(result, X_OK) == 0)
            return 1;
    }
    return 0;
}

void execute_command(char *buf) {
    if (buf[0] == '\0' || buf[0] == '#')
        return;

    // tokenize
    char *tokens[MAX_TOKENS];
    int num_tokens = 0;
    char *token = strtok(buf, " \t");
    while (token != NULL && token[0] != '#') {
        tokens[num_tokens++] = token;
        token = strtok(NULL, " \t");
    }
    tokens[num_tokens] = NULL;

    if (num_tokens == 0)
        return;

    // built-in: exit
    if (strcmp(tokens[0], "exit") == 0) {
        write(STDOUT_FILENO, "Exiting my shell.\n", 18);
        exit(EXIT_SUCCESS);

    // built-in: cd
    } else if (strcmp(tokens[0], "cd") == 0) {
        char *dir;
        if (num_tokens == 1)
            dir = getenv("HOME");
        else if (num_tokens == 2)
            dir = tokens[1];
        else {
            write(STDOUT_FILENO, "cd: too many arguments\n", 23);
            return;
        }
        if (chdir(dir) != 0)
            perror("cd");

    // built-in: pwd
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[BUF_SIZE];
        getcwd(cwd, sizeof(cwd));
        write(STDOUT_FILENO, cwd, strlen(cwd));
        write(STDOUT_FILENO, "\n", 1);

    // external command: fork + execv
    } else {
        char path[BUF_SIZE];

        // if it contains '/', use it directly, otherwise search
        if (strchr(tokens[0], '/') != NULL) {
            strncpy(path, tokens[0], BUF_SIZE);
        } else if (!find_path(tokens[0], path)) {
            write(STDOUT_FILENO, "Command not found\n", 18);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) {
            // child: execute the program
            execv(path, tokens);
            perror("execv");
            exit(EXIT_FAILURE);
        } else {
            // parent: wait for child to finish
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

void input_loop(int interactive) {
	char buf[BUF_SIZE];
	char cwd[BUF_SIZE]; // working directory
	char *home = getenv("HOME"); // '~'
	int pos;
	char c;

	while (1) {
		getcwd(cwd, sizeof(cwd));

		if (interactive) {

			if (home != NULL && strncmp(cwd, home, strlen(home)) == 0) {
				write(STDOUT_FILENO, "~", 1);
				write(STDOUT_FILENO, cwd + strlen(home), strlen(cwd + strlen(home)));
			} else {
				write (STDOUT_FILENO, cwd, strlen(cwd));
			}
			write(STDOUT_FILENO, "$ ", 2);
		}

		pos = 0;
		while (1) {
			int n = read(STDIN_FILENO, &c, 1);

			if (n < 0) {
				perror("read");
				exit(EXIT_FAILURE);
			}

			if (n == 0) {
				write(STDOUT_FILENO, "\nExiting my shell.\n", 19);
				return;
			}

			if (c == '\n') {
				buf[pos] = '\0';
				break;
			}

			buf[pos++] = c;
			if (pos >= BUF_SIZE - 1) {
				buf[pos] = '\0';
				break;
			}
		}

		// executing commands
		if (buf[0] == '\0' || buf[0] == '#') {
			continue;	// empty line or comment
		}
		else if (strcmp(buf, "exit") == 0) {
			write(STDOUT_FILENO, "Exiting my shell.\n", 18);
			exit(EXIT_SUCCESS);
		} else {
			execute_command(buf);
			write(STDOUT_FILENO, "\n", 1);
		}
	}
}


int main(void) {
	int interactive = 1;
	if (interactive)
		write(STDOUT_FILENO, "Welcome to my shell.\n", 21);

	input_loop(interactive);

	return EXIT_SUCCESS;
}
