/* BSD */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#undef LIST_HEAD
#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif

#ifdef HAVE_LIBKVM
extern bool can_use_kvm;
extern kvm_t *kd;
#endif

struct pinfo {
	int pid;
	int ppid;
};

void machine_init ();
int get_login_pid (const char *tty);
void for_each_pinfo (void (*func) (struct pinfo *info, void *data),void *data);
