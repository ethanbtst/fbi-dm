#ifndef PAM_H
#define PAM_H
#include <security/pam_appl.h>
#include <pwd.h>
#include <stdbool.h>

// Performs multiple functions to check the provided password
// Will (1) pam_start (2) pam_authenticate (3) pam_acct_mgmt
// If all checks out, returns true
bool check_user(const char *uname, const char *passwd, pam_handle_t **pamh);

// Sets up user environment
bool setup_env(pam_handle_t *pamh, struct passwd *pw);
#endif