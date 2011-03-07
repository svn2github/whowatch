#include <string>
#include <vector>

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "machine.h"

struct proc_info {
	int pid;
	int ppid;
};

static std::vector<proc_info> g_proc_infos;

bool update_proc_all ()
{
	DIR *d = opendir("/proc");
	if (!d) return false;

	while (true) {
		struct dirent* e = readdir (d);
		if (!e) break;

		if (!isdigit(e->d_name[0])) continue;

		std::string name =
			std::string("/proc/") + e->d_name + "/stat";

		int fd = open(name.c_str(), 0);
		if (!fd) continue;

		char buf[64];
		int n = read(fd, buf, 63);
		close(fd);
		if (n < 0) continue;

		buf[n] = '\0';
		char *p = strrchr(buf + 4, ')') + 4;

		struct proc_info info;
		info.ppid = atoi(p);
		info.pid = atoi(buf);
		if (info.pid <= 0) continue;

		g_proc_infos.push_back(info);
	}

	closedir(d);
	return true;
}

int proc_numbers ()
{
        return g_proc_infos.size();
}

int proc_pid (int i)
{
        return g_proc_infos[i].pid;
}

int proc_ppid (int i)
{
        return g_proc_infos[i].ppid;
}
