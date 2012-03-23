/*
 * Functions needed for printing process owner in the tree.
 */
#include "config.h"
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>

#define NAME_SIZE	8

char *get_owner_name (int uid)
{
	static char name[NAME_SIZE + 1];
	struct passwd *u;

	u = getpwuid (uid);
	if (u) {
		return u->pw_name;
	}

	snprintf (name, NAME_SIZE + 1, "%d", uid);
	return name;
}
