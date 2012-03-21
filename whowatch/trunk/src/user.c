#include "config.h"
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#include "whowatch.h"

#define LOGIN		1
#define LOGOUT		-1		

LIST_HEAD(users_l);
static int toggle;	/* if 0 show cmd line else show idle time 	*/

char *line_buf;		/* global buffer for line printing		*/
int buf_size;		/* allocated buffer size			*/

struct prot_t
{
	char *s;
	short port;
	unsigned int nr;
};

/* indexes in prot_tab[] */
#define	SSH		0
#define TELNET		1
#define LOCAL		2

static struct prot_t prot_tab[] = {
#ifdef HAVE_PROCESS_SYSCTL
	{ "sshd", 22, 0 }, { "telnetd", 23, 0 }, { "init", -1, 0 }
#else
	{ "(sshd", 22, 0 }, { "(in.telnetd)", 23, 0 }, { "(init)", -1, 0 }
#endif
};


/*  
 *  update number of users (ssh users, telnet users...) and change 
 *  prot in the appropriate user structure.
 */
void u_count(char *name, int p)
{
	int i;
	struct prot_t *t;
	users_list.d_lines += p;
	dolog("%s : dlines %d\n", __FUNCTION__, users_list.d_lines);	
	for(i = 0; i < sizeof prot_tab/sizeof(struct prot_t); i++){
		t = &prot_tab[i];
		if(strncmp(t->s, name, strlen(t->s))) continue;
		t->nr += p;
	}
}

/*
 * After deleting line, update line numbers in each user structure 
 */
void update_line(int line)
{
	struct user_t *u;
	struct list_head *tmp;
	list_for_each(tmp, &users_l) {
		u = list_entry(tmp, struct user_t, head); 
		if(u->line > line) u->line--;
	}	
}
			
/* 
 * Create new user structure and fill it
 */
struct user_t *alloc_user(struct utmpx *entry)
{
	struct user_t *u;
	int ppid;
	u = calloc(1, sizeof *u);
	if(!u) errx(1, "Cannot allocate memory.");
	strncpy (u->name, entry->ut_user, sizeof(entry->ut_user));
	strncpy (u->tty, entry->ut_line, sizeof(entry->ut_line));
#ifdef HAVE_STRUCT_UTMPX_UT_HOST
	strncpy(u->host, entry->ut_host, sizeof(entry->ut_host));
#endif
	u->pid = entry->ut_pid;
 	if((ppid = get_ppid(u->pid)) == -1)
		strncpy(u->parent, "can't access", sizeof u->parent);
	else 	strncpy(u->parent, get_name(ppid), sizeof u->parent - 1);
	
	u->line = users_list.d_lines;
	return u;
}

static struct user_t* new_user(struct utmpx *ut)
{
	struct user_t *u;
	u = alloc_user(ut);
	list_add(&u->head, &users_l);
	u_count(u->parent, LOGIN);
	return u;
}
	
static void print_user(struct user_t *u)
{
	wattrset(users_list.wd, A_BOLD);
	snprintf(line_buf, buf_size, 
		"%-14.14s %-9.9s %-6.6s %-19.19s %s",
		u->parent, u->name, u->tty, u->host, 
		toggle?count_idle(u->tty):get_w(u->pid));
	line_buf[buf_size - 1] = 0;
	print_line(&users_list, line_buf , u->line, 0);
}

void users_list_refresh(void)
{
	struct list_head *tmp;
	struct user_t *u;
	list_for_each_r(tmp, &users_l) {
		u = list_entry(tmp, struct user_t, head);
		if(above(u->line, &users_list)) continue;
		if(below(u->line, &users_list)) break;
		print_user(u);
	}
}
	
/*
 * Gather informations about users currently on the machine
 * Called only at start or restart
 */
static void read_utmp(void)		
{
	int fd, i;
	struct utmpx *entry;
	struct user_t *u;
	
	while ((entry = getutxent()) != NULL) {
	  if (entry->ut_type == USER_PROCESS) {
	    u = new_user (entry);
	    //		print_user(u);
	  }
	}

//	wnoutrefresh(users_list.wd);
	return;
}


/*
 * get user entry from specific line (cursor position)
 */
struct user_t *cursor_user(void)	
{
	struct user_t *u;
	struct list_head *h;
	int line = current->cursor + current->offset;
	list_for_each(h, &users_l) {
		u = list_entry(h, struct user_t, head);
		if(u->line == line) return u;
	}
	return 0;
}

static void del_user(struct user_t *u)
{
	delete_line(&users_list, u->line);
	update_line(u->line);
	u_count(u->parent, LOGOUT);
	list_del(&u->head);
	free(u);
}


void print_info(void)
{
        char buf[128];
	int other = users_list.d_lines - prot_tab[LOCAL].nr - 
		prot_tab[TELNET].nr - prot_tab[SSH].nr;
werase(info_win.wd);
        snprintf(buf, sizeof buf - 1,
                "\x1%d users: (%d local, %d telnet, %d ssh, %d other)",
                users_list.d_lines, prot_tab[LOCAL].nr, prot_tab[TELNET].nr, prot_tab[SSH].nr, other);
        echo_line(&info_win, buf, 0);
        wnoutrefresh(info_win.wd);
}

/*
 * Check wtmp for logouts or new logins
 */
void check_wtmp (void)
{
	struct user_t *u;
	struct list_head *h;
	struct utmpx *entry;
	int i, show, changed;
	show = changed = 0;
	if(current == &users_list) show = 1;
	while ((entry = getutxent()) != NULL) {
		/* user just logged in */
		if (entry->ut_type == USER_PROCESS) {
			u = new_user (entry);
			changed = 1;
			continue;
		}
		if (entry->ut_type == DEAD_PROCESS) {
		  /* user just logged out */
		  list_for_each(h, &users_l) {
		    u = list_entry(h, struct user_t, head);
		    if(strncmp(u->tty, entry->ut_line, sizeof(entry->ut_line)))
		      continue;
		    del_user(u);	
		    changed = 1;
		    break;
		  }
		}
	}
	if(!changed) return;
	if(show) users_list_refresh();
	print_info();
}

char *users_list_giveline(int line)
{
	struct user_t *u;
	struct list_head *h;	
	list_for_each(h, &users_l) {
		u = list_entry(h, struct user_t, head);
		if (line != u->line) continue;
		snprintf(line_buf, buf_size, 
			"%-14.14s %-9.9s %-6.6s %-19.19s %s", 
			u->parent, u->name, u->tty, u->host, 
				toggle?count_idle(u->tty):get_w(u->pid));
		return line_buf;
	}
	return "not available";
}

static void cmdline(void)
{
        struct window *q = &users_list;
        struct user_t *u;
        struct list_head *h;
        int y, x;

	if(CMD_COLUMN >= screen_cols) return;
        list_for_each(h, &users_l) {
                u = list_entry(h, struct user_t, head);
//                if(u->line < q->offset ||
  //                      u->line > q->offset + q->rows - 1)
		if(outside(u->line, q)) continue;
		wmove(q->wd, u->line - q->offset, CMD_COLUMN);
                cursor_off(q, q->cursor);
                wattrset(q->wd, A_BOLD);
                wmove(q->wd, u->line - q->offset, CMD_COLUMN);
                waddnstr(q->wd, toggle?count_idle(u->tty):get_w(u->pid),
                         COLS - CMD_COLUMN - 1);
                getyx(q->wd, y, x);
                while(x++ < q->cols + 1)
                        waddch(q->wd, ' ');
                cursor_on(q, q->cursor);
                wmove(q->wd, q->cursor, q->cols + 1);
        }
}


static bool ulist_key(int key)
{
	struct user_t *u = 0;
	pid_t pid = INIT_PID;
	
        switch(key) {
        case KEY_ENTER:
		u = cursor_user();
		if(u) pid = u->pid; 
        case 't':
		werase(main_win);
		current = &proc_win;
		print_help();
		show_tree(pid);
		tree_title(u);
		sub_switch();
		pad_draw();
		break;
        case 'i':
                toggle ^= 1;
                cmdline();
                break;
        default:
	        return KEY_SKIPPED;
        }
}

static void periodic(void)
{
	cmdline();
}

void users_init(void)
{
	users_list.giveme_line = users_list_giveline;
	users_list.keys = ulist_key;
	users_list.periodic = periodic;
	users_list.redraw = users_list_refresh;

	setutxent ();
	read_utmp();
	endutxent ();

	// open wtmp database
#ifdef HAVE_UTMPNAME
	if (utmpname (_PATH_WTMP) == -1) {
	  err(1, "%s: cannot open wtmp database",
	      __FUNCTION__);
	}
#else
	if (setutxdb (UTXDB_LOG, NULL) == -1) {
	  err(1, "%s: cannot open wtmp database",
	      __FUNCTION__);
	}
#endif
	setutxent ();
	// skip to end
	while (getutxent()) ;

	print_info();
}

/* 
 * Needed for search function. If parent, name, tty, host or command line
 * matches then returns line number of this user.
 */
unsigned int user_search(int line)
{
	struct user_t *u;
	struct list_head *h;
	
	list_for_each(h, &users_l) {
		u = list_entry(h, struct user_t, head);
		if(u->line < line) continue;
		if(reg_match(u->parent)) return u->line;
		if(reg_match(u->name)) return u->line;
		if(reg_match(u->tty)) return u->line;
		if(reg_match(u->host)) return u->line;
		if(reg_match(toggle?count_idle(u->tty):get_w(u->pid))) 
			return u->line;
	}
	return -1;
}


