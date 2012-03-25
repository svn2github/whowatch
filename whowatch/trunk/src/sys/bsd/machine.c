/* BSD */

#include "config.h"

#include <fcntl.h>

#include "machine.h"

#ifdef HAVE_LIBKVM
bool can_use_kvm = false;
kvm_t *kd;
#endif

void machine_init ()
{
#ifdef HAVE_LIBKVM
	kd = kvm_openfiles(0, 0, 0, O_RDONLY, 0);
	if (!kd) {
	  can_use_kvm = false;
	}
	else {
	  can_use_kvm = true;
	}
#endif
}
