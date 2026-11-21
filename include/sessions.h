#ifndef SESSIONS_H
#define SESSIONS_H

#include <stddef.h>
#include <pwd.h>

typedef struct {
	char *name;
	char *exec;
	char *type;
} session;

extern int session_row;
extern int session_col;
extern int session_cur;
extern size_t session_count;
extern session *sessions;

session *get_sessions(size_t *session_count);
session copy_session(const session *source);
void free_session(session *session_info);
void free_sessions(session *session_list, size_t count);
void start_session(struct passwd *pw, const session *selected);
int get_session_index(int session_count);
void set_session_index(int session_cur);

#endif