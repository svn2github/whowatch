/*
 *  Jan Bobrowski <jb@wizard.ae.krakow.pl>
 *  version: 20000511
 */

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "machine.h"
#include "proctree.h"

#define HASHSIZE 128

static void list_broth_add (struct proc_t *&l, struct proc_t *p)
{
	p->broth.nx = l;
	p->broth.ppv = &l;
	if (l) l->broth.ppv = &(p->broth.nx);
	l = p;
}
		
static void list_hash_add (struct proc_t *&l, struct proc_t *p)
{
	p->hash.nx = l;
	p->hash.ppv = &l;
	if (l) l->hash.ppv = &(p->hash.nx);
	l = p;
}

static void list_broth_del (struct proc_t *p)
{
	*p->broth.ppv = p->broth.nx;
	if (p->broth.nx) p->broth.nx->broth.ppv = p->broth.ppv;
}

static void list_hash_del (struct proc_t *p)
{
	*p->hash.ppv = p->hash.nx;
	if (p->hash.nx) p->hash.nx->hash.ppv = p->hash.ppv;
}

static bool is_on_list_broth (struct proc_t *p)
{
	return p->broth.ppv != 0;
}

#define proc_zero (proc_special[0])
#define proc_init (proc_special[1])
static struct proc_t proc_special[2] = { (0), (1) };
static struct proc_t *hash_table[HASHSIZE];
static list_proc_t_mlist main_list;
static int num_proc = 1;

static inline int hash_fun(int n)
{
	return n&(HASHSIZE-1);
}

struct proc_t* find_by_pid (int pid)
{
	struct proc_t* p;
	if (pid <= 1) return &proc_special[pid];

	p = hash_table[hash_fun(pid)];
	while(p) {
		if (p->pid == pid) break;
		p = p->hash.nx;
	}
	return p;
}

static inline void remove_proc(struct proc_t* p)
{
	list_hash_del (p);
	num_proc--;
}

static inline struct proc_t* new_proc (int pid)
{
	struct proc_t* p = (struct proc_t*) calloc(sizeof *p, 1);
	p->pid = pid;

	list_hash_add (hash_table[hash_fun(pid)], p);
	num_proc++;

	return p;
}

static struct proc_t *validate_proc (int pid)
{
	struct proc_t* p;

	p = find_by_pid(pid);
	if(pid <= 1)
		return p;
	if(p)
		main_list.erase(main_list.iterator_to(*p));
	else
		p = new_proc(pid);
	main_list.push_front(*p);
	return p;
}

static inline void change_parent(struct proc_t* p, struct proc_t* q)
{
	if(is_on_list_broth (p))
		list_broth_del (p);
	list_broth_add (q->child, p);
	p->parent = q;
}

int update_tree (void del(void*))
{
	list_proc_t_mlist old_list;
	int n = num_proc;
	main_list.swap(old_list);
	
	if (!update_proc_all())
		return -1;

	for (int i = 0; i < proc_numbers(); i++) {
	  struct proc_t *p = validate_proc(proc_pid(i));
	  struct proc_t *q = validate_proc(proc_ppid(i));

	  if (p->parent != q) {
		  if (p->priv) del(p->priv);
		  change_parent(p,q);
	  }
	}

	n = num_proc - n;

	typedef list_proc_t_mlist::iterator I;
	for (I p = old_list.begin(); p != old_list.end(); ) {
	        while (p->child)
			change_parent(p->child,&proc_init);
		if(is_on_list_broth(&*p))
			list_broth_del (&*p);
		I q = p;
		q++;
		if (p->priv) del(p->priv);
		remove_proc(&*p);
		n++;
		p = q;
	}

	return n;
}

/* ---------------------- */

static struct proc_t *proc, *root;

struct proc_t* tree_start(int root_pid, int start_pid)
{
	root = find_by_pid(root_pid);
	if (!root) return 0;
	proc = find_by_pid(start_pid);
	if(start_pid)
		return proc;
	return tree_next();	/* skip zero proc - it doesn't really exist */
}

struct proc_t* tree_next()
{
	if(proc->child)
		proc = proc->child;
	else
		for(;; proc = proc->parent) {
			if(proc == root)
				proc = 0;
			else if(proc->broth.nx)
				proc = proc->broth.nx;
			else
				continue;
			break;
		}
	return proc;
}

static char buf[TREE_STRING_SZ];

char* tree_string(int root, struct proc_t *p)
{
	struct proc_t *q;
	char *s;
	int i, d;

	i = 0;
	for(q=p; q->pid!=root; q=q->parent)
		i++;

	if(root == 0)
		i--;	/* forest of all processes case */
	d = i;

	while(i > TREE_DEPTH)
		p=p->parent, i--;

	s = buf + 2*i;
	s[1] = 0;
	s[0] = '.';

	if(d <= TREE_DEPTH) {
		s[0] = '-';
		if(i > 0 && !p->broth.nx) {
			*--s = '`';
			goto loop;
		}
	}

	while(i > 0) {
		*--s = p->broth.nx ? '|' : ' ';
loop:
		*--s = ' ';
		p=p->parent, i--;
	}

	return buf;
}
