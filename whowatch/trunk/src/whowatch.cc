#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "whowatch.h"
#include "config.h"

#ifdef DEBUG
FILE *g_debug_file;
#endif

#define TIMEOUT 	3

unsigned long long g_ticks;	/* increased every TIMEOUT seconds	*/
struct window g_users_list;
struct window g_proc_win;
struct window *g_current;
static int size_changed; 
bool g_full_cmd = true;	/* if 1 then show full cmd line in tree		*/
static int signal_sent;
int g_screen_rows;	/* screen rows returned by ioctl  		*/
int g_screen_cols;	/* screen cols returned by ioctl		*/
char *g_line_buf;		/* global buffer for line printing		*/
int g_buf_size;		/* allocated buffer size			*/

//enum key { ENTER=KEY_MAX + 1, ESC, CTRL_K, CTRL_I };

struct key_handler {
	enum key c;
	void (*handler)(struct window *);
};

/*
 * These are key bindings to handle cursor movement.
 */
static struct key_handler key_handlers[] = {
	{ (key)KBD_UP, cursor_up	 	},
	{ (key)KBD_DOWN, cursor_down 	},
	{ (key)KBD_PAGE_DOWN, page_down	},
	{ (key)KBD_PAGE_UP, page_up		},
	{ (key)KBD_HOME, key_home		},
	{ (key)KBD_END, key_end		}
};  

/*
 * Functions for key handling. They have to be called in proper order.
 * Function associated with object that is on top has to be called first.
 */
static int (*key_funct[])(int) = {
	info_box_keys,
	box_keys,
	menu_keys,
	sub_keys,
};
	

#ifdef HAVE_LIBKVM
int kvm_init();
int can_use_kvm = 0;
#endif

void prg_exit(char *s)
{
	curses_end();
	if(s) printf("%s\n", s);
	exit(0);
}

void allocate_error(){
	curses_end();
	fprintf(stderr,"Cannot allocate memory.\n");
	exit (1);
}
 
void update_load();

/*
 * Process these function after each TIMEOUT (default 3 seconds).
 * Order is important because some windows are on the top
 * of others.
 */
static void periodic(void)
{
	check_wtmp();
	update_load();		
	g_current->periodic();
	wnoutrefresh(g_main_win);
	wnoutrefresh(g_info_win.wd);
	sub_periodic();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

void send_signal(int sig, pid_t pid)
{
	int p;
	char buf[64];
	/*  Protect init process */
	if(pid == INIT_PID) p = -1;
	else p = kill(pid, sig);
	signal_sent = 1;
	if(p == -1)
		sprintf(buf,"Can't send signal %d to process %d",
			sig, pid); 
	else sprintf(buf,"Signal %d was sent to process %d",sig, pid);
	werase(g_help_win.wd);
	echo_line(&g_help_win, buf, 0);
	wnoutrefresh(g_help_win.wd);
}

void m_search(void);
void help(void);

static void key_action(int key)
{
	int size;
	if (signal_sent) {
		print_help();
		signal_sent = 0;
	}
	/* 
	 * First, try to process the key by object (subwindow, menu) that
	 * could be on top.
	 */
	size = sizeof key_funct/sizeof(int (*)(int));
	for (int i = 0; i < size; i++)
		if(key_funct[i](key)) goto SKIP; 
	
	if(g_current->keys(key)) goto SKIP;
	/* cursor movement */
	size = sizeof key_handlers/sizeof(struct key_handler);
	for (int i = 0; i < size; i++) 
		if (key_handlers[i].c == key) {
			key_handlers[i].handler(g_current);
			if (can_draw()) pad_draw();
			goto SKIP;
		}
	switch(key) {
	case 'c':
		g_full_cmd = !g_full_cmd;
		g_current->redraw();
		break;
	case '/':
		m_search();
		break;			
	case KBD_F1:
		help();
		break;
	case KBD_ESC:
	case 'q':
		curses_end();
	        exit(0);
	default: return;
	}
SKIP:
	dolog("%s: doing refresh\n", __FUNCTION__);
	wnoutrefresh(g_main_win);
	wnoutrefresh(g_info_win.wd);
	pad_refresh();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

static void get_rows_cols(int *y, int *x)
{
	struct winsize win;
	if (ioctl(1,TIOCGWINSZ,&win) != -1){
                *y = win.ws_row;
		*x = win.ws_col;
		return;
	}
	prg_exit("get_row_cols(): ioctl error: cannot read screen size.");
}								

static void winch_handler (int unused)
{
	size_changed++;
}

/* 
 * Handle SIGWINCH. Order of calling various resize
 * functions is really important.
 */
static void resize(void)
{
	get_rows_cols(&g_screen_rows, &g_screen_cols);
	resizeterm(g_screen_rows, g_screen_cols);
	wresize(g_main_win, g_screen_rows-3, g_screen_cols);
	win_init();
	mvwin(g_help_win.wd, g_screen_rows - 1, 0);	
//	wnoutrefresh(help_win.wd);
//	wnoutrefresh(info_win.wd);
	/* set the cursor position if necessary */
	if(g_current->cursor > g_current->rows)
		g_current->cursor = g_current->rows; 
	werase(g_main_win);
	g_current->redraw();
	wnoutrefresh(g_main_win);                                             
	print_help();
	pad_resize();
	print_info();
	update_load();
	menu_resize();
	box_resize();
	info_resize();
	doupdate();
	size_changed = 0;
}

static void int_handler (int unused)
{
	curses_end();
	exit(0);
}		

int main (int argc, char **argv)
{
	struct timeval tv;

#ifdef HAVE_LIBKVM
	if (kvm_init()) can_use_kvm = 1;
#endif
	
#ifdef DEBUG
	if (!(debug_file = fopen("debug", "w"))) {
		printf("file debug open error\n");
		exit(0);
	}
#endif
	get_boot_time();
	get_rows_cols(&g_screen_rows, &g_screen_cols);
	g_buf_size = g_screen_cols + g_screen_cols/2;
	g_line_buf = (char*)malloc(g_buf_size);
	if (!g_line_buf)
		errx(1, "Cannot allocate memory for buffer.");

	curses_init();
	g_current = &g_users_list;
	users_init();
	procwin_init();
	subwin_init();
	menu_init();
	signal(SIGINT, int_handler);
	signal(SIGWINCH, winch_handler);  
	//	signal(SIGSEGV, segv_handler);

	print_help();
	update_load();
	g_current->redraw();
	wnoutrefresh(g_main_win);
	wnoutrefresh(g_info_win.wd);
	wnoutrefresh(g_help_win.wd);
	doupdate();
	
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	
	for(;;) {				/* main loop */
#ifndef RETURN_TV_IN_SELECT
		struct timeval before;
		struct timeval after;
#endif
		fd_set rfds;
		int retval;
		
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO,&rfds);
#ifdef RETURN_TV_IN_SELECT
		retval = select(1, &rfds, 0, 0, &tv);
		if(retval > 0) {
			int key = read_key();
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			g_ticks++;
			periodic();
			tv.tv_sec = TIMEOUT;
		}
#else
		gettimeofday(&before, 0);
		retval = select(1, &rfds, 0, 0, &tv);
		gettimeofday(&after, 0);
		tv.tv_sec -= (after.tv_sec - before.tv_sec);
		if(retval > 0) {
			int key = read_key();
			key_action(key);
		}
		if(tv.tv_sec <= 0) {
			g_ticks++;
			periodic();
			tv.tv_sec = TIMEOUT;

		}
#endif
		if (size_changed) resize();
	}
}
