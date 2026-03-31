#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024

void input_loop(int interactive) {
	char buf[BUF_SIZE];
	int pos;
	char c;

	while (1) {
		if (interactive)
		       write(STDOUT_FILENO, "$ ", 2);

		pos = 0;
		while (1) {
			int n = read(STDIN_FILENO, &c, 1);

			if (n < 0) {
				perror("read");
				exit(EXIT_FAILURE);
			}

			if (n == 0) {
				if (interactive)
					write (STDOUT_FILENO, "Exiting my shell.\n", 18);
				return;
			}

			if (c == '\n') {
				buf[pos] = '\0';
				break;
			}
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
