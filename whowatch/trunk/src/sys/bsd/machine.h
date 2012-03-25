/* BSD */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif

#ifdef HAVE_LIBKVM
extern bool can_use_kvm;
extern kvm_t *kd;
#endif

void machine_init ();
int get_login_pid (const char *tty);
int get_all_info (struct kinfo_proc **);
