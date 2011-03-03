/*
 *  Jan Bobrowski <jb@wizard.ae.krakow.pl>
 *  version: 20000511
 */

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "config.h"


#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#endif

#include "proctree.h"

#define PROCDIR "/proc"
#define HASHSIZE 128

static void list_mlist_add (struct proc_t *&l, struct proc_t *p)
{
	p->mlist.nx = l;
	p->mlist.ppv = &l;
	if (l) l->mlist.ppv = &(p->mlist.nx);
	l = p;
}
		
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

static void list_mlist_del (struct proc_t *p)
{
	*p->mlist.ppv = p->mlist.nx;
	if (p->mlist.nx) p->mlist.nx->mlist.ppv = p->mlist.ppv;
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

static void change_head_mlist (struct proc_t *list, struct proc_t *old)
{
	old = list;
	if (list) list->mlist.ppv = &old;
}

#ifdef HAVE_PROCESS_SYSCTL
int get_all_info(struct kinfo_proc **);
#endif 

struct pinfo {
	int pid;
	int ppid;
};

#ifndef HAVE_PROCESS_SYSCTL
static inline int get_pinfo(struct pinfo* i,DIR* d)
{
	static char name[32] = PROCDIR "/";
	struct dirent* e;

	for(;;) {
		int f,n;
		char buf[64],*p;

		e=readdir(d);
		if(!e) return 0;
		if(!isdigit(e->d_name[0])) continue;
		sprintf(name+sizeof PROCDIR,"%s/stat",e->d_name);
		f=open(name,0);
		if(!f) continue;
		n=read(f,buf,63);
		close(f);
		if(n<0) continue;
		buf[n]=0;
		p = strrchr(buf+4,')') + 4;
		i->ppid = atoi(p);
		i->pid = atoi(buf);
		if(i->pid<=0) continue;
		break;
	}
	return 1;
}
#endif

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

struct proc_t* find_by_pid(int n)
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
	list_hash_del (p);
	num_proc--;
}

static inline struct proc_t* new_proc(int n)
{
	struct proc_t* p = (struct proc_t*) malloc(sizeof *p);
	memset(p,0,sizeof *p);
	p->pid = n;

	list_hash_add (hash_table[hash_fun(n)], p);
	num_proc++;

	return p;
}

static struct proc_t *validate_proc(int pid)
{
	struct proc_t* p;

	p = find_by_pid(pid);
	if(pid <= 1)
		return p;
	if(p)
		list_mlist_del (p);
	else
		p = new_proc(pid);
	list_mlist_add (main_list, p);
	return p;
}

static inline void change_parent(struct proc_t* p,struct proc_t* q)
{
	if(is_on_list_broth (p))
		list_broth_del (p);
	list_broth_add (q->child, p);
	p->parent = q;
}

int update_tree(void del(void*))
{
#ifdef HAVE_PROCESS_SYSCTL
	struct kinfo_proc *pi;
	int i, el;
#else
	struct pinfo info;
	DIR *d;
#endif
	struct proc_t *p,*q;
	struct proc_t *old_list;
	int n = num_proc;
	change_head_mlist (main_list, old_list);
	main_list = 0;
	
#ifdef HAVE_PROCESS_SYSCTL
	el = get_all_info(&pi);
	for(i = 0; i < el; i++) {
		p = validate_proc(pi[i].kp_proc.p_pid);
		q = validate_proc(pi[i].kp_eproc.e_ppid);
#else

	d=opendir(PROCDIR);
	if(d<0) return -1;

	while( get_pinfo(&info,d) ) {
		p = validate_proc(info.pid);
		q = validate_proc(info.ppid);
#endif
		if(p->parent != q){
			if(p->priv) del(p->priv);

			change_parent(p,q);
		}
	}
#ifdef HAVE_PROCESS_SYSCTL
	free(pi);
#else
	closedir(d);
#endif
	n = num_proc - n;

	for(p = old_list; p; p=q) {
		while(p->child)
			change_parent(p->child,&proc_init);
		if(is_on_list_broth(p))
			list_broth_del (p);
		q = p->mlist.nx;
		if(p->priv) del(p->priv);
		remove_proc(p);
		n++;
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
