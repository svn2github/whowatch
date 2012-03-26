#include <stdbool.h>
#include <sys/select.h>
#include <utmpx.h>

#include <curses.h>

#include "kbd.h"
#include "list.h"

#define member_size(type, member) sizeof(((type *)0)->member)

#define CURSOR_COLOR	A_REVERSE
#define NORMAL_COLOR	A_NORMAL
#define CMD_COLUMN	52

#define KEY_SKIPPED	false
#define KEY_HANDLED	true

#define INIT_PID		1
#define real_line_nr(x,y)	((x) - (y)->offset)

/* whowatch.c */
extern unsigned long long ticks;
extern bool full_cmd;
extern int screen_rows;
extern int screen_cols;
extern char *line_buf;
extern int buf_size;

/* screen.c */
extern WINDOW *main_win;

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
  char name[member_size(struct utmpx, ut_user) + 1];    /* login name   */
  char tty[member_size(struct utmpx, ut_line) + 1];     /* tty          */
  int pid;                              /* pid of login shell           */
  char parent[16];                      /* login shell parent's name	*/
#ifdef HAVE_STRUCT_UTMPX_UT_HOST
  char host[member_size(struct utmpx, ut_host) + 1];
#else
  char host[1];
#endif
  int line;                             /* line number                  */
};

/* whowatch.c */
extern struct window users_list;
extern struct window proc_win;
extern struct window *current;

/* screen.c */
extern struct window help_win;
extern struct window info_win;

struct process
{
        struct process **prev;
        struct process *next;
        int line;
	char state;
	int uid;
	struct proc_t *proc;
};

/* help.c */
void help ();

/* user.c */
void users_init(void);
void check_wtmp(void);
void print_info(void);
struct user_t *cursor_user(void);
unsigned int user_search(int);
void users_list_refresh();

/* whowatch.c */
void send_signal (int, pid_t);

/* process.c */
void show_tree(pid_t);
void procwin_init(void);
pid_t cursor_pid(void);
unsigned int getprocbyname(int);
void tree_title(struct user_t *);
void do_signal(int, int);

/* screen.c */								
bool below(int, struct window *);
bool above(int, struct window *);
bool outside(int, struct window *);
void win_init(void);
int print_line(struct window *w, const char *s, int line, bool virtual);
int echo_line(struct window *w, const char *s, int line);
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
void update_tree (void (*del) (void*));

/* procinfo.c */
char *get_cmdline(int);
int get_ppid(int);
char *get_name(int);
char *get_w(int pid);
void get_state(struct process *p);
char *count_idle (const char *tty);
void get_boot_time(void);

/* owner.c */
char *get_owner_name(int u);

/* subwin.c */
bool sub_keys(int);
void subwin_init(void);
void sub_periodic(void);
void pad_refresh(void);
void pad_draw(void);
void pad_resize(void);
void new_sub(void(*)(void *));
char *plugin_load (const char*);
bool can_draw(void);
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
void do_search (const char *);
bool reg_match (const char *);

/* menu_hooks.c */
void set_search(char *);
void m_search ();

/* kbd.c */
int read_key ();

/* util.c */
#ifndef RETURN_TV_IN_SELECT
int _select (int nfds, fd_set *readfds, fd_set *writefds,
	    fd_set *exceptfds, struct timeval *timeout);
#define select _select
#endif
void* xmalloc (size_t size);
void* xcalloc (size_t nmemb, size_t size);
void *xrealloc (void *ptr, size_t size);
void dolog (const char *format, ...);
