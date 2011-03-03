#include <boost/intrusive/list.hpp>

#define TREE_DEPTH 32
#define TREE_STRING_SZ (2 + 2*TREE_DEPTH)

struct plist {
	struct proc_t* nx;   // pointer to next element
	struct proc_t** ppv; // pointer to next element pointer of the
			     // previous element
};

struct tag_mlist;
typedef boost::intrusive::list_member_hook<boost::intrusive::tag<tag_mlist> > list_member_hook_mlist;

struct proc_t
{
	proc_t () {}
	proc_t (int _pid) : pid(_pid) {}

	int pid;
	struct proc_t *parent;
	struct proc_t *child;
	list_member_hook_mlist hook_mlist;
	struct plist broth;
	struct plist hash;
	void* priv;
};

typedef boost::intrusive::member_hook<proc_t,
	list_member_hook_mlist,
	&proc_t::hook_mlist> member_hook_mlist;

typedef boost::intrusive::list<proc_t,
	member_hook_mlist> list_proc_t_mlist;

int update_tree(void del(void*));
int update_tree();
struct proc_t* find_by_pid(int pid);
struct proc_t* tree_start(int root, int start);
struct proc_t* tree_next();
char *tree_string(int root, struct proc_t *proc);
