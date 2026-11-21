#include "term_utils.h"
#include "sessions.h"
#include "info.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

cursor *cur = &(cursor){ 1, 1 };
struct termios current_term;
struct termios saved_term;

void toggle_echo(bool enabled)
{
	if (enabled)
		current_term.c_lflag |= ECHO;
	else
		current_term.c_lflag &= ~ECHO;

	tcsetattr(STDIN_FILENO, TCSANOW, &current_term);
}

void setup_term()
{
	printf("\033[2J\033[H");
	move(100, 2);
	fflush(stdout);

	tcgetattr(STDIN_FILENO, &current_term);
	saved_term = current_term;
	current_term.c_lflag &= ~(ICANON | ECHO | ISIG);
	current_term.c_cc[VMIN] = 1;
	current_term.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &current_term);
}

void move_cur(cursor *cur, int row, int col)
{
	cur->col = col;
	cur->row = row;
	printf("\033[%d;%dH", row, col);
}

cursor get_cur(void)
{
	cursor position = { 0, 0 };
	unsigned char ch;

	printf("\033[6n");
	fflush(stdout);

	while (read(STDIN_FILENO, &ch, 1) == 1) {
		if (ch != '\033')
			continue;

		if (read(STDIN_FILENO, &ch, 1) == 1 && ch == '[') {
			int row = 0;
			int col = 0;

			if (read(STDIN_FILENO, &ch, 1) == 1) {
				while (ch >= '0' && ch <= '9') {
					row = row * 10 + (ch - '0');
					if (read(STDIN_FILENO, &ch, 1) != 1)
						return position;
				}

				if (ch == ';') {
					if (read(STDIN_FILENO, &ch, 1) != 1)
						return position;
					while (ch >= '0' && ch <= '9') {
						col = col * 10 + (ch - '0');
						if (read(STDIN_FILENO, &ch, 1) != 1)
							return position;
					}
				}

				if (ch == 'R') {
					position.row = row;
					position.col = col;
					return position;
				}
			}
		}
	}

	return position;
}

int get_row(void)
{
	return get_cur().row;
}

int get_col(void)
{
	return get_cur().col;
}

void update_session(int change)
{
	if (sessions == NULL)
		return;

	*cur = get_cur();
	cursor saved = *cur;
	move(session_row, session_col);
	clear_line();

	int session_total = (int)session_count + 1;
	session_cur = (session_cur + change) % session_total;
	if (session_cur < 0)
		session_cur += session_total;
	session_info(sessions, session_cur, session_count);

	move(saved.row, saved.col);
	fflush(stdout);
}

void readline(cursor *cur, char *buff)
{
	int ch;
	while ((ch = getchar()) != '\n') {
		if (ch >= 32 && ch <= 126) {
			strcat(buff, (char *)&ch);
		} else if (ch == '\b' || ch == KEY_BACKSPACE) {
			if (strnlen(buff, sizeof(buff)) == 0)
				continue;
			buff[strlen(buff) - 1] = '\0';
		}
	}
	buff[strlen(buff)] = '\0';
	cur->row = cur->row + 1;
	cur->col = cur->col + strlen(buff);
}

int readkey(void)
{
	unsigned char ch;
	unsigned char sequence[2];
	unsigned char suffix[2];

	if (read(STDIN_FILENO, &ch, 1) != 1)
		return EOF;

	if (ch != '\033')
		return ch;

	if (read(STDIN_FILENO, sequence, 2) != 2)
		return ch;

	if (sequence[0] == 'O' && sequence[1] == 'P')
		return KEY_F1;
	if (sequence[0] == 'O' && sequence[1] == 'Q')
		return KEY_F2;
	if (sequence[0] == 'O' && sequence[1] == 'R')
		return KEY_F3;
	if (sequence[0] == 'O' && sequence[1] == 'S')
		return KEY_F4;
	if (sequence[0] == '[' && sequence[1] == 'D')
		return KEY_LEFT;
	if (sequence[0] == '[' && sequence[1] == 'C')
		return KEY_RIGHT;
	if (sequence[0] == '[' && sequence[1] == '[') {
		if (read(STDIN_FILENO, &ch, 1) != 1)
			return '\033';
		if (ch == 'A')
			return KEY_F1;
		if (ch == 'B')
			return KEY_F2;
		if (ch == 'C')
			return KEY_F3;
		if (ch == 'D')
			return KEY_F4;
		return '\033';
	}
	if (sequence[0] == '[' && sequence[1] == '1' &&
		read(STDIN_FILENO, suffix, sizeof(suffix)) == sizeof(suffix) &&
		suffix[1] == '~') {
		if (suffix[0] == '1')
			return KEY_F1;
		if (suffix[0] == '2')
			return KEY_F2;
		if (suffix[0] == '3')
			return KEY_F3;
		if (suffix[0] == '4')
			return KEY_F4;
	}
	return ch;
}

void key_loop(input_buffer *input_buffers, size_t input_count)
{
	size_t cur_buff = 0;
	printf("%s: ", input_buffers[0].name);
	fflush(stdout);
	no_echo();
	*cur = get_cur();

	while (cur_buff < input_count) {
		int ch = readkey();

		switch (ch) {
		case '\n':
			cur_buff++;
			if (cur_buff < input_count) {
				printf("\n%s: ", input_buffers[cur_buff].name);
				fflush(stdout);
			} else {
				printf("\n");
				fflush(stdout);
				echo();
			}
			if (cur_buff < input_count)
				*cur = get_cur();
			break;

		case '\b':
		case KEY_BACKSPACE: {
			char *buffer = input_buffers[cur_buff].buffer;
			size_t length =
				strnlen(buffer, sizeof(input_buffers[cur_buff].buffer));
			if (length > 0) {
				buffer[length - 1] = '\0';
				if (input_buffers[cur_buff].show) {
					printf("\b \b");
					fflush(stdout);
				}
				*cur = get_cur();
			}
			break;
		}

		case KEY_LEFT:
			update_session(1);
			break;

		case KEY_RIGHT:
			update_session(-1);
			break;

		case KEY_F1:
			system("poweroff");
			break;

		case KEY_F2:
			system("reboot");
			break;

		case KEY_F3:
			echo();
			if (opt_brightness)
				update_brightness(1);
			no_echo();
			break;

		case KEY_F4:
			echo();
			if (opt_brightness)
				update_brightness(-1);
			no_echo();
			break;

		default:
			if (ch >= 32 && ch <= 126) {
				char *buffer = input_buffers[cur_buff].buffer;
				size_t length =
					strnlen(buffer, sizeof(input_buffers[cur_buff].buffer));
				if (length + 1 < sizeof(input_buffers[cur_buff].buffer)) {
					buffer[length] = (char)ch;
					buffer[length + 1] = '\0';
					if (input_buffers[cur_buff].show) {
						putchar(ch);
						fflush(stdout);
					}
					*cur = get_cur();
				}
			}
			break;
		}
	}
}

void cleanup(image *images[], int image_count, struct termios *term,
			 buffer *buffer)
{
	for (int i = 0; i < image_count; ++i) {
		if (images[i] != NULL) {
			free(images[i]->data);
			images[i]->data = NULL;
		}
	}

	free_sessions(sessions, session_count);
	sessions = NULL;
	session_count = 0;

	destroy_buffer(buffer);
	tcsetattr(STDIN_FILENO, TCSANOW, term);
}
