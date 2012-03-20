#include <pwd.h>

#include "machine.h"

bool uid_to_name (int uid, std::string &name)
{
	struct passwd *u = getpwuid (uid);
	if (!u) return false;

	name =u->pw_name;
	return true;
}

