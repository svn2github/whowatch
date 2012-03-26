/*
 *  Jan Bobrowski <jb@wizard.ae.krakow.pl>
 *  version: 20000511
 */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "whowatch.h"
#include "machine.h"
#include "proctree.h"

#define HASHSIZE 128

#define list_add(l,p,f) ({			\
	(p)->f.nx = (l);			\
	(p)->f.ppv = &(l);			\
	if(l) (l)->f.ppv = &((p)->f.nx);	\
	(l) = (p);				\
})
		
#define list_del(p,f) ({				\
	*(p)->f.ppv = (p)->f.nx;			\
	if((p)->f.nx) (p)->f.nx->f.ppv = (p)->f.ppv;	\
})

#define is_on_list(p,f) (!!(p)->f.ppv)

#define change_head(a,b,f) ({b=a; if(a) a->f.ppv=&b;})

#define proc_zero (proc_special[0])
#define proc_init (proc_special[1])
static struct proc_t proc_special[2] = {{0},{1}};
static struct proc_t *hash_table[HASHSIZE];
static struct proc_t *main_list = 0;
static int num_proc = 1;

static inline int hash_fun(int n)
{
	return n&(HASHSIZE-1);
}

static struct proc_t* find_by_pid(int n)
{
	struct proc_t* p;
	if(n<=1) return &proc_special[n];

	p = hash_table[hash_fun(n)];
	while(p) {
		if(p->pid == n) break;
		p=p->hash.nx;
	}
	return p;
}

static inline void remove_proc(struct proc_t* p)
{
	list_del(p,hash);
	num_proc--;
}

static inline struct proc_t* new_proc(int n)
{
        struct proc_t* p;

	p = (struct proc_t*) xcalloc (1, sizeof *p);
	p->pid = n;

	list_add(hash_table[hash_fun(n)], p, hash);
	num_proc++;

	return p;
}

static struct proc_t *validate_proc(int pid)
{
	struct proc_t* p;

	p = find_by_pid(pid);
	if (pid <= 1) {
		return p;
	}
	if (p) {
		list_del(p,mlist);
	}
	else {
		p = new_proc(pid);
	}
	list_add(main_list,p,mlist);
	return p;
}

static inline void change_parent(struct proc_t* p,struct proc_t* q)
{
	if (is_on_list(p,broth)) {
		list_del(p,broth);
	}
	list_add(q->child,p,broth);
	p->parent = q;
}

void update_tree_helper (struct pinfo *ptr, void *data)
{
  void (*del) (void*) = (void (*) (void *)) data;
  struct proc_t *p = validate_proc (ptr->pid);
  struct proc_t *q = validate_proc (ptr->ppid);

  if (p->parent != q) {
    if (p->priv) del (p->priv);
    change_parent (p, q);
  }
}

void update_tree (void (*del) (void*))
{
  struct proc_t *p,*q;
  struct proc_t *old_list;
  change_head (main_list, old_list,mlist);
  main_list = 0;

  for_each_pinfo (&update_tree_helper, (void*)del);

  for (p = old_list; p != NULL; p = q) {
    while (p->child) {
      change_parent(p->child,&proc_init);
    }
    if (is_on_list(p,broth)) {
      list_del(p,broth);
    }
    q = p->mlist.nx;
    if(p->priv) del(p->priv);
    remove_proc(p);
  }
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
	if (proc->child) {
		proc = proc->child;
	}
	else {
		for(;; proc = proc->parent) {
			if(proc == root)
				proc = 0;
			else if(proc->broth.nx)
				proc = proc->broth.nx;
			else
				continue;
			break;
		}
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
