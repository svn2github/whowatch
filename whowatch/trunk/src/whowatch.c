#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "whowatch.h"
#include "machine.h"

#define TIMEOUT 	3

unsigned long long ticks;	/* increased every TIMEOUT seconds	*/
bool full_cmd = true;	/* if 1 then show full cmd line in tree		*/
int screen_rows;	/* screen rows returned by ioctl  		*/
int screen_cols;	/* screen cols returned by ioctl		*/
char *line_buf;		/* global buffer for line printing		*/
int buf_size;		/* allocated buffer size			*/

struct window users_list;
struct window proc_win;
struct window *current;

static bool size_changed; 
static bool signal_sent;

struct key_handler {
        int key;
	void (*handler)(struct window *);
};

/*
 * These are key bindings to handle cursor movement.
 */
static struct key_handler key_handlers[] = {
	{ KEY_UP, cursor_up	 	},
	{ KEY_DOWN, cursor_down 	},
	{ KEY_NPAGE, page_down	},
	{ KEY_PPAGE, page_up		},
	{ KEY_HOME, key_home		},
	{ KEY_END, key_end		}
};  

/*
 * Functions for key handling. They have to be called in proper order.
 * Function associated with object that is on top has to be called first.
 */
static bool (*key_funct[])(int) = {
	info_box_keys,
	box_keys,
	menu_keys,
	sub_keys,
};
	

/*
 * Process these function after each TIMEOUT (default 3 seconds).
 * Order is important because some windows are on the top
 * of others.
 */
static void periodic(void)
{
	check_wtmp();
	update_load();		
	current->periodic();
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
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
	signal_sent = true;
	if(p == -1)
		sprintf(buf,"Can't send signal %d to process %d",
			sig, pid); 
	else sprintf(buf,"Signal %d was sent to process %d",sig, pid);
	werase(help_win.wd);
	echo_line(&help_win, buf, 0);
	wnoutrefresh(help_win.wd);
}

static void key_action (int key)
{
	int i, size;
	if(signal_sent) {
	    print_help();
	    signal_sent = false;
	}

	/* 
	 * First, try to process the key by object (subwindow, menu) that
	 * could be on top.
	 */
	size = sizeof key_funct/sizeof(int (*)(int));
	for(i = 0; i < size; i++)
		if(key_funct[i](key)) goto SKIP; 
	
	if(current->keys(key)) goto SKIP;
	/* cursor movement */
	size = sizeof key_handlers/sizeof(struct key_handler);
	for(i = 0; i < size; i++) 
		if(key_handlers[i].key == key) {
			key_handlers[i].handler(current);
			if(can_draw()) pad_draw();
			goto SKIP;
		}
	switch(key) {
	case 'c':
		full_cmd = !full_cmd;
		current->redraw();
		break;
	case '/':
		m_search();
		break;			
	case KEY_F(1):
		help();
		break;
	case KEY_ESC:
	case 'q':
		exit(EXIT_SUCCESS);
	default: return;
	}
SKIP:
	dolog("%s: doing refresh\n", __FUNCTION__);
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
	pad_refresh();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

static void get_rows_cols (int *y, int *x)
{
  struct winsize win;

  if (ioctl(1,TIOCGWINSZ,&win) == -1) {
    err(EXIT_FAILURE, "get_row_cols(): ioctl error: cannot read screen size.");
  }
  *y = win.ws_row;
  *x = win.ws_col;
}								

static void winch_handler()
{
	size_changed = true;
}

/* 
 * Handle SIGWINCH. Order of calling various resize
 * functions is really important.
 */
static void resize(void)
{
	get_rows_cols(&screen_rows, &screen_cols);
	resizeterm(screen_rows, screen_cols);
	wresize(main_win, screen_rows-3, screen_cols);
	win_init();
	mvwin(help_win.wd, screen_rows - 1, 0);	
//	wnoutrefresh(help_win.wd);
//	wnoutrefresh(info_win.wd);
	/* set the cursor position if necessary */
	if(current->cursor > current->rows)
		current->cursor = current->rows; 
	werase(main_win);
	current->redraw();
	wnoutrefresh(main_win);                                             
	print_help();
	pad_resize();
	print_info();
	update_load();
	menu_resize();
	box_resize();
	info_resize();
	doupdate();
	size_changed = false;
}

static void int_handler()
{
	exit(EXIT_SUCCESS);
}		

int main (int argc, char **argv)
{
	struct timeval tv;

	machine_init ();
	get_boot_time();
	get_rows_cols(&screen_rows, &screen_cols);
	buf_size = screen_cols + screen_cols/2;
	line_buf = xmalloc(buf_size);

	curses_init();
	current = &users_list;
	users_init();
	procwin_init();
	subwin_init();
	menu_init();
	signal(SIGINT, int_handler);
	signal(SIGWINCH, winch_handler);  
	//	signal(SIGSEGV, segv_handler);

	print_help();
	update_load();
	current->redraw();
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
	wnoutrefresh(help_win.wd);
	doupdate();
	
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	
	for(;;) {				/* main loop */
		fd_set rfds;
		int retval;
		
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO,&rfds);
		retval = select (STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

		if (retval > 0) {
			int key = read_key();
			if (key != ERR) key_action(key);
		}
		if ((tv.tv_sec == 0) && (tv.tv_usec == 0)) {
			ticks++;
			periodic();
			tv.tv_sec = TIMEOUT;
		}
		if (size_changed) resize();
	}
}
