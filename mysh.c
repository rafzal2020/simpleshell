#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024

void execute_command(char *buf) {
	if (buf[0] == '\0' || buf[0] == '#') {
		return;	// empty line or comment
	}
	else if (strcmp(buf, "exit") == 0) {
		write(STDOUT_FILENO, "Exiting my shell.\n", 18);
		exit(EXIT_SUCCESS);
	} else {
		write(STDOUT_FILENO, "Command not yet implemented.\n", 28);
	}
}

void input_loop(int interactive) {
	char buf[BUF_SIZE];
	char cwd[BUF_SIZE]; // working directory
	char *home = getenv("HOME"); // '~'
	int pos;
	char c;

	getcwd(cwd, sizeof(cwd));

	while (1) {
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
