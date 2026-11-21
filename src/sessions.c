
#include "sessions.h"
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "main_utils.h"

int session_row = 0;
int session_col = 0;
int session_cur;
size_t session_count;
session *sessions;

session *get_sessions(size_t *session_count)
{
	glob_t paths = { 0 };
	*session_count = 0;

	int glob_result = glob("/usr/share/xsessions/*", GLOB_ERR, NULL, &paths);
	if (glob_result == GLOB_NOMATCH)
		glob_result = 0;
	if (glob_result != 0) {
		globfree(&paths);
		paths = (glob_t){ 0 };
	}

	glob_result = glob("/usr/share/wayland-sessions/*", GLOB_ERR | GLOB_APPEND,
					   NULL, &paths);
	if (glob_result == GLOB_NOMATCH)
		glob_result = 0;
	if (glob_result != 0) {
		globfree(&paths);
		paths = (glob_t){ 0 };
	}

	session *sessions = calloc(paths.gl_pathc + 1, sizeof(*sessions));
	if (sessions == NULL) {
		perror("Failed to allocate sessions");
		globfree(&paths);
		return NULL;
	}

	size_t count = paths.gl_pathc;

	for (size_t i = 0; i < count; ++i) {
		FILE *fd = fopen(paths.gl_pathv[i], "r");
		if (!fd) {
			perror("Failed to open session");
			continue;
		}

		sessions[i] = (session){ 0 };
		char line[255];
		char *option;
		char *value;

		while (fgets(line, sizeof(line), fd)) {
			if (!parse_line(line, &option, &value))
				continue;
			if (strcasecmp(option, "exec") == 0) {
				free(sessions[i].exec);
				sessions[i].exec = malloc(strlen(value) + 1);
				if (sessions[i].exec == NULL) {
					perror("Failed to allocate session exec");
					break;
				}
				strcpy(sessions[i].exec, value);
			}
			if (strcasecmp(option, "name") == 0) {
				free(sessions[i].name);
				sessions[i].name = malloc(strlen(value) + 1);
				if (sessions[i].name == NULL) {
					perror("Failed to allocate session name");
					break;
				}
				strcpy(sessions[i].name, value);
			}
		}
		if (strstr(paths.gl_pathv[i], "/usr/share/wayland-sessions/") != NULL) {
			sessions[i].type = malloc(strlen("wayland") + 1);
			if (sessions[i].type == NULL) {
				perror("Failed to allocate session type");
				fclose(fd);
				free_sessions(sessions, count);
				globfree(&paths);
				*session_count = 0;
				return NULL;
			}
			strcpy(sessions[i].type, "wayland");
		} else {
			sessions[i].type = malloc(strlen("x11") + 1);
			if (sessions[i].type == NULL) {
				perror("Failed to allocate session type");
				fclose(fd);
				free_sessions(sessions, count);
				globfree(&paths);
				*session_count = 0;
				return NULL;
			}
			strcpy(sessions[i].type, "x11");
		}
		fclose(fd);
	}

	globfree(&paths);
	sessions[count].name = malloc(strlen("Shell") + 1);
	sessions[count].exec = NULL;
	sessions[count].type = malloc(strlen("Other") + 1);
	if (sessions[count].name == NULL || sessions[count].type == NULL) {
		free_session(&sessions[count]);
		free_sessions(sessions, count);
		globfree(&paths);
		*session_count = 0;
		perror("Failed to allocate shell session");
		return NULL;
	}
	strcpy(sessions[count].name, "Shell");
	strcpy(sessions[count].type, "Other");
	*session_count = count;
	return sessions;
}

session copy_session(const session *source)
{
	session copy = { 0 };
	if (source == NULL)
		return copy;

	if (source->name != NULL) {
		copy.name = malloc(strlen(source->name) + 1);
		if (copy.name == NULL)
			goto allocation_error;
		strcpy(copy.name, source->name);
	}
	if (source->exec != NULL) {
		copy.exec = malloc(strlen(source->exec) + 1);
		if (copy.exec == NULL)
			goto allocation_error;
		strcpy(copy.exec, source->exec);
	}
	if (source->type != NULL) {
		copy.type = malloc(strlen(source->type) + 1);
		if (copy.type == NULL)
			goto allocation_error;
		strcpy(copy.type, source->type);
	}
	return copy;

allocation_error:
	free_session(&copy);
	return (session){ 0 };
}

void free_session(session *session_info)
{
	if (session_info == NULL)
		return;

	free(session_info->exec);
	free(session_info->type);
	free(session_info->name);
	*session_info = (session){ 0 };
}

void free_sessions(session *session_list, size_t count)
{
	if (session_list == NULL)
		return;

	for (size_t i = 0; i < count + 1; ++i)
		free_session(&session_list[i]);
	free(session_list);
}

void start_session(struct passwd *pw, const session *selected)
{
	if (pw == NULL || selected == NULL || selected->type == NULL)
		return;

	if (chdir(pw->pw_dir) != 0) {
		perror("chdir");
		exit(EXIT_FAILURE);
	}

	if (strcasecmp(selected->type, "other") == 0 ||
		strcasecmp(selected->type, "shell") == 0) {
		execl(pw->pw_shell, pw->pw_shell, "--login", (char *)NULL);
		perror("execl");
		exit(EXIT_FAILURE);
	}

	const char *session_type = selected->type;
	if (strcasecmp(session_type, "x11") == 0)
		setenv("XDG_SESSION_TYPE", "x11", 1);
	else if (strcasecmp(session_type, "wayland") == 0)
		setenv("XDG_SESSION_TYPE", "wayland", 1);

	setenv("XDG_SESSION_DESKTOP", selected->name, 1);
	setenv("XDG_CURRENT_DESKTOP", selected->name, 1);
	setenv("DESKTOP_SESSION", selected->name, 1);

	if (selected->exec == NULL || selected->exec[0] == '\0') {
		fprintf(stderr, "Selected session has no Exec entry\n");
		exit(EXIT_FAILURE);
	}

	if (strcasecmp(session_type, "x11") == 0) {
		execl("/usr/bin/startx", "startx", "/bin/sh", "-c", selected->exec,
			  (char *)NULL);
		perror("execl");
		exit(EXIT_FAILURE);
	}

	execl("/bin/sh", "sh", "-c", selected->exec, (char *)NULL);
	perror("execl");
	exit(EXIT_FAILURE);
}

int get_session_index(int session_count)
{
	FILE *fd = fopen("/opt/fb-dm/index.txt", "r");
	if (!fd)
		return -1;

	int i;
	fscanf(fd, "%d", &i);
	fclose(fd);

	if (i >= 0 && i <= session_count)
		return i;
	else
		return -1;
}

void set_session_index(int session_cur)
{
	FILE *fd = fopen("/opt/fb-dm/index.txt", "w");
	if (!fd)
		return;

	fprintf(fd, "%d", session_cur);
	fclose(fd);
}