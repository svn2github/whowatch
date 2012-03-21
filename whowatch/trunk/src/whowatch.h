#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <assert.h>
#include "list.h"
#include "kbd.h"

#define CURSOR_COLOR	A_REVERSE
#define NORMAL_COLOR	A_NORMAL
#define CMD_COLUMN	52

#define KEY_SKIPPED	false
#define KEY_HANDLED	true

#define INIT_PID		1
#define real_line_nr(x,y)	((x) - (y)->offset)

extern FILE *debug_file;
extern int screen_rows, screen_cols;
extern char *line_buf;
extern int buf_size;
extern unsigned long long ticks;
extern WINDOW *main_win;
extern int full_cmd;

/*
 * Data associated with window are line numbered. If scrolling
 * inside window occurs then number of first displayed line changes
 * (first_line). 
 */
struct window
{
	unsigned int rows;
	unsigned int cols;
	int offset;		/* nr of first displayed line	 	*/
	int s_col;		/* display starts from this col		*/
	int d_lines;		/* current total number of data lines 	*/
	int cursor;		/* cursor position		 	*/
	WINDOW *wd;		/* curses window pointer		*/
	char *(*giveme_line) (int line);
	bool (*keys)(int c);	/* keys handling 			*/
	void (*periodic)(void);	/* periodic updates of window's data 	*/
	void (*redraw)(void);	/* refreshes window content		*/
};

struct user_t
{
        struct list_head head;
        char name[UT_NAMESIZE + 1];     /* login name                   */
        char tty[UT_LINESIZE + 1];      /* tty                          */
        int pid;                        /* pid of login shell           */
        char parent[16];                /* login shell parent's name	*/
        char host[UT_HOSTSIZE + 1];     /* 'from' host                  */
        int line;                       /* line number                  */
};

extern struct window users_list, proc_win, help_win, info_win;
extern struct window *current;

struct process
{
        struct process **prev;
        struct process *next;
        int line;
	char state;
	int uid;
	struct proc_t *proc;
};

/* user.c */
void users_init(void);
void check_wtmp(void);
void print_info(void);
struct user_t *cursor_user(void);
unsigned int user_search(int);
void users_list_refresh();

/* whowatch.c */
void allocate_error();
void prg_exit(char *);
void send_signal(int, pid_t);

/* process.c */
void show_tree(pid_t);
void procwin_init(void);
pid_t cursor_pid(void);
unsigned int getprocbyname(int);
void tree_title(struct user_t *);
void do_signal(int, int);

/* screen.c */								
int below(int, struct window *);
int above(int, struct window *);
int outside(int, struct window *);
void win_init(void);
int print_line(struct window *w, char *s, int line, int virtual);
int echo_line(struct window *w, char *s, int line);
void cursor_on(struct window *w, int line);
void cursor_off(struct window *w, int line);
void curses_init();
void curses_end();
void print_help(void);
void cursor_down(struct window *w);
void cursor_up(struct window *w);
void delete_line(struct window *w, int line);
void page_down(struct window *);
void page_up(struct window *);
void key_home(struct window *);
void key_end(struct window *);
void update_load(void);
void to_line(int, struct window *);

/* proctree.c */
int update_tree();

/* procinfo.c */
#ifdef HAVE_PROCESS_SYSCTL
int get_login_pid(char *tty);
#endif
char *get_cmdline(int);
int get_ppid(int);
char *get_name(int);
char *get_w(int pid);
void delete_tree_line(void *line);
void get_state(struct process *p);
char *count_idle(char *tty);
#ifndef HAVE_GETLOADAVG
int getloadavg(double [], int);
#endif
void proc_details(int);
void sys_info(int);
void get_boot_time(void);

/* owner.c */
char *get_owner_name(int u);

/* block.c */
void *get_empty(int, struct list_head *);
int free_entry(void *, int, struct list_head *);
void dolog(const char *, ...);

/* subwin.c */
bool sub_keys(int);
void subwin_init(void);
void sub_periodic(void);
void pad_refresh(void);
void pad_draw(void);
void pad_resize(void);
void new_sub(void(*)(void *));
char *plugin_load(char*);
int can_draw(void);
void sub_switch(void);


/* input_box.c */
bool box_keys(int);
void box_refresh(void);
void box_resize(void);
void input_box(char *, char *, char *,void (*)(char *));

/* menu.c */
void menu_refresh(void);
bool menu_keys(int);
void menu_init(void);
void menu_resize(void);

/* info_box.c */
void info_box(char *, char *);
bool info_box_keys(int);
void info_refresh(void);
void info_resize(void);

/* proc_plugin.c */
void builtin_proc_draw(void *);
void builtin_sys_draw(void *);

/* user_plugin.c */
void builtin_user_draw(void *);

/* search.c */
void do_search(char *);
int reg_match(const char *);

/* menu_hooks.c */
//void clear_search(void);
void set_search(char *);

/* kbd.c */
int getkey();
int read_key ();

/* term.c */
void term_raw();
