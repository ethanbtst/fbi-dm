#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pam.h"

struct login_data {
	const char *username;
	const char *password;
};

static int custom_conv(int num_msg, const struct pam_message **msg,
					   struct pam_response **resp, void *appdata_ptr)
{
	if (num_msg <= 0 || msg == NULL || resp == NULL || appdata_ptr == NULL)
		return PAM_CONV_ERR;

	const struct login_data *data = appdata_ptr;
	struct pam_response *reply = calloc(num_msg, sizeof(struct pam_response));
	if (!reply)
		return PAM_BUF_ERR;

	for (int i = 0; i < num_msg; i++) {
		if (msg[i] == NULL)
			goto conversation_error;

		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			reply[i].resp = strdup(data->password);
			break;
		case PAM_PROMPT_ECHO_ON:
			reply[i].resp = strdup(data->username);
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		default:
			goto conversation_error;
		}

		if ((msg[i]->msg_style == PAM_PROMPT_ECHO_OFF ||
			 msg[i]->msg_style == PAM_PROMPT_ECHO_ON) &&
			reply[i].resp == NULL)
			goto conversation_error;
	}

	*resp = reply;
	return PAM_SUCCESS;

conversation_error:
	for (int i = 0; i < num_msg; i++)
		free(reply[i].resp);

	free(reply);
	return PAM_CONV_ERR;
}

bool check_user(const char *uname, const char *passwd, pam_handle_t **pamh)
{
	if (uname == NULL || passwd == NULL || *uname == '\0')
		return false;
	if (pamh == NULL)
		return false;

	struct login_data data = { uname, passwd };
	struct pam_conv conv = { custom_conv, &data };
	*pamh = NULL;

	int retval = pam_start("login", uname, &conv, pamh);
	if (retval != PAM_SUCCESS) {
		printf("PAM start failed: %s\n", pam_strerror(NULL, retval));
		*pamh = NULL;
		return false;
	}

	char *tty = ttyname(STDIN_FILENO);
	if (tty != NULL)
		retval = pam_set_item(*pamh, PAM_TTY, tty);
	if (retval == PAM_SUCCESS)
		retval = pam_authenticate(*pamh, 0);
	if (retval == PAM_SUCCESS)
		retval = pam_acct_mgmt(*pamh, 0);

	if (retval != PAM_SUCCESS) {
		pam_end(*pamh, retval);
		*pamh = NULL;
		return false;
	}
	return true;
}

bool setup_env(pam_handle_t *pamh, struct passwd *pw)
{
	pam_set_item(pamh, PAM_TTY, ttyname(STDIN_FILENO));

	/* Import PAM environment (includes XDG_RUNTIME_DIR from pam_systemd.so) */
	char **pam_env = pam_getenvlist(pamh);
	if (pam_env) {
		for (size_t i = 0; pam_env[i] != NULL; i++)
			putenv(pam_env[i]); /* putenv takes ownership of the string */

		free(pam_env);
	}

	if (setenv("HOME", pw->pw_dir, 1) != 0)
		return false;
	if (setenv("USER", pw->pw_name, 1) != 0)
		return false;
	if (setenv("LOGNAME", pw->pw_name, 1) != 0)
		return false;
	if (setenv("SHELL", pw->pw_shell, 1) != 0)
		return false;
	if (setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1) != 0)
		return false;

	return true;
}
