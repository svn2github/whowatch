#include <vector>

#include "config.h"
#include "machine.h"

#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#endif



static std::vector<struct kinfo_proc> g_proc_infos;

/*
 * Get information about all system processes
 */
bool update_proc_all ()
{
	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	int el;
	size_t len;

	if (sysctl(mib, 3, 0, &len, 0, 0) == -1)
	  return false;
	el = len / sizeof(struct kinfo_proc);
	g_proc_infos.resize (el);
	if (sysctl(mib, 3, &g_proc_infos[0], &len, 0, 0) == -1)
		return false;
	return true;
}

int proc_numbers ()
{
        return g_proc_infos.size();
}

int proc_pid (int i)
{
        return g_proc_infos[i].ki_pid;
}

int proc_ppid (int i)
{
        return g_proc_infos[i].ki_ppid;
}
