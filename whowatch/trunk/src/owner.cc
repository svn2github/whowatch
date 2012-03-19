/*
 * Functions needed for printing process owner in the tree.
 */

#include <string>
#include <boost/unordered_map.hpp>

#include "config.h"
#include "machine.h"
#include "whowatch.h"

typedef boost::unordered_map<int, std::string> hash_table;
static hash_table g_uid_name_map;

std::string get_owner_name (int uid)
{
	hash_table::iterator i;

	i = g_uid_name_map.find (uid);
	if (i != g_uid_name_map.end()) {
		return i->second;
	}

	std::string name;
	if (!uid_to_name (uid, name)) {
		name = int_to_string (uid);
	}
	g_uid_name_map[uid] = name;
	return name;
}
