/* 
 * Menu implementation. All bindings are defined in menu_hooks.c
 * Menu/submenu definition is static - see item_bind[].
 */

#include "whowatch.h"
#include "menu_hooks.h"

#define TITLE_START	2	/* left margin inside menu bar */
#define TITLE_STEP	4
#define MSIZE_X		32	/* size of a submenu window */
#define MSZIE_Y		32

static WINDOW *submenu_wd;

static struct menu_t {
	WINDOW *wd;
	struct list_head submenus;
	unsigned short cols;
} menu;	

struct submenu_t {
	struct list_head l_menu;
	struct list_head items;
	char *title;
	unsigned short rows, cols, coord_x;
};

struct item_t {
	struct list_head l_submenu;
	char *name, *descr;
	void (*hook)(void);
};	

static struct submenu_t *cur_submenu;
static struct item_t *cur_item;
static unsigned short item_cursor;

#define DUMMY_HEAD	{0, 0}
static char *submenus[] = {
  "File", "View", "Process", "Users",  "Help", NULL
};

struct item_bind_t  {
	unsigned short submenu;		/* index in the submenus table */
	struct item_t item;
};

static struct item_bind_t item_bind[] = {
	{ 0, { DUMMY_HEAD , " Load plugin", "", m_load_plugin } } ,
	{ 0, { DUMMY_HEAD , " Exit", "q ", m_exit } } ,
	{ 1, { DUMMY_HEAD , " Search", "/ ", m_search } } ,
	{ 1, { DUMMY_HEAD , " All processes", "t ", m_process } } ,
	{ 1, { DUMMY_HEAD , " Users", "Ent ", m_switch } } ,
	{ 1, { DUMMY_HEAD , " User proc", "Ent ", m_switch } } ,
	{ 1, { DUMMY_HEAD , " Details", "d ", m_details } } ,
	{ 1, { DUMMY_HEAD , " Sysinfo", "s ", m_sysinfo } } ,
	{ 2, { DUMMY_HEAD , " Toggle owner", "o ", m_owner } } ,
	{ 2, { DUMMY_HEAD , " Toggle long", "c ", m_long } } ,
	{ 2, { DUMMY_HEAD , " Signal list", "l ", m_siglist } } ,
	{ 2, { DUMMY_HEAD , " Send KILL", "^K ", m_kill } } ,
	{ 2, { DUMMY_HEAD , " Send HUP", "^U ", m_hup } } ,
	{ 2, { DUMMY_HEAD , " Send TERM", "^T ", m_term } } ,
	{ 3, { DUMMY_HEAD , " Toggle idle", "i ", m_idle } } ,
	{ 3, { DUMMY_HEAD , " Toggle long", "c ", m_long } } ,
	{ 4, { DUMMY_HEAD , " Keys", "F1 ", help } } ,
	{ 4, { DUMMY_HEAD , " About", "", m_about } } ,
	{ 4, { DUMMY_HEAD , " Copyright", "", 0 } } ,
};

/*
 * Add menu to the menu bar 
 */
static struct submenu_t *add_submenu(char *s)
{
	struct submenu_t *t;
	static int pos = TITLE_START;
	
	t = calloc(1, sizeof *t);
	if(!t) prg_exit("add_submenu(): cannot allocate memory.");
	t->title = s;
	INIT_LIST_HEAD(&t->items);
	list_add(&t->l_menu, &menu.submenus);
	t->coord_x = pos;
	pos += strlen(s) + TITLE_STEP;
	return t;
}

static struct submenu_t *find_submenu(char *title)
{
	struct list_head *h;
	struct submenu_t *t;
	list_for_each(h, &menu.submenus) {
		t = list_entry(h, struct submenu_t, l_menu);
		if(!strncasecmp(t->title, title, strlen(title))) 
			return t;
	}
	return 0;
}			

static void add_item(char *title, struct item_t *i)
{
	struct submenu_t *t;
	static unsigned short longest;
	int len = strlen(i->name);// + strlen(i->descr) + 1;
	if(!(t = find_submenu(title))) {
	// dolog("%s: cannot find title %s\n", __FUNCTION__, title);
		return;
	}
	if(len > longest) longest = len;
	// dolog("%s: %d %d %d\n", __FUNCTION__, longest, strlen(i->descr), t->cols);	
	if(longest + strlen(i->descr) + 3 > t->cols) 
		t->cols = 3 + longest + strlen(i->descr);
	if (t->rows == 0) t->rows = 3; /* make space for a border line */
	else t->rows++;
	list_add(&i->l_submenu, &t->items);
}

static void submenu_print(struct submenu_t *t)
{
	struct list_head *h;
	struct item_t *i;
	int x = 1;

	list_for_each_r(h, &t->items) {
		i = list_entry(h, struct item_t, l_submenu);
		mvwaddstr(submenu_wd, x, 1, i->name);
		mvwaddstr(submenu_wd, x++, t->cols - strlen(i->descr) - 1, i->descr);
	}
}

static void submenu_refresh(struct submenu_t *t)
{
assert(submenu_wd != NULL);
assert(cur_submenu != NULL);
	pnoutrefresh(submenu_wd, 0, 0, 1, t->coord_x, t->rows, t->cols + t->coord_x);
}

void menu_refresh(void)
{
	if(!menu.wd) return;
	pnoutrefresh(menu.wd, 0, 0, 0, 0, 1, menu.cols);
	if(submenu_wd) submenu_refresh(cur_submenu);
}

static void set_size(void)
{
	menu.cols = screen_cols;
}

/* 
 * Print menu titles inside main menu bar.
 */
static void titles_print(void)
{
	struct list_head *h;
	int pos = 2;
	struct submenu_t *t;
	list_for_each_r(h, &menu.submenus) {
		t = list_entry(h, struct submenu_t, l_menu);
		if(cur_submenu == t) wattrset(menu.wd, A_REVERSE);
		else wattrset(menu.wd, A_NORMAL);
		mvwaddstr(menu.wd, 0, pos + TITLE_START - 2, "  ");
		waddstr(menu.wd, t->title);
		waddstr(menu.wd, "  ");
		pos += strlen(t->title) + TITLE_STEP;
	}
}	

static void submenu_create(struct submenu_t *t)
{
assert(t != NULL);
	submenu_wd = newpad(t->rows, t->cols);
	if(!submenu_wd) prg_exit("Cannot create ncurses pad.");
	wbkgd(submenu_wd, COLOR_PAIR(9));
	werase(submenu_wd);
 	box(submenu_wd, ACS_VLINE, ACS_HLINE);	
}	


void menu_init(void)
{
	int i;
	struct item_t *it;
	
	INIT_LIST_HEAD(&menu.submenus);
	for(i = 0; submenus[i]; i++)
		add_submenu(submenus[i]);
	for(i = 0; i < sizeof item_bind/sizeof(struct item_bind_t); i++) {
		it = &item_bind[i].item;
		add_item(submenus[item_bind[i].submenu], it);
	}	
}

static void menu_create(void)
{
	set_size();
	menu.wd = newpad(1, menu.cols);
	if(!menu.wd) prg_exit("menu_create(): Cannot allocate memory.");
	wbkgd(menu.wd, COLOR_PAIR(9));
//	overwrite(info_win.wd, menu.wd);
	werase(menu.wd);
//	wattrset(menu.wd,A_REVERSE);
	cur_submenu = find_submenu("file");
	titles_print();
}

static void menu_destroy()
{
	delwin(menu.wd);
	if(submenu_wd) delwin(submenu_wd);
	menu.wd = submenu_wd = 0;
	item_cursor = 0;
	cur_item = 0;
	redrawwin(main_win);
	redrawwin(info_win.wd);
}

static void highlight_item(struct submenu_t *t, int i)
{
  if (item_cursor != 0) { 
    mvwchgat(submenu_wd, item_cursor, 1, t->cols-2, A_NORMAL, 9, 0);
  }
  item_cursor += i;
  mvwchgat(submenu_wd, item_cursor, 1, t->cols-2, A_REVERSE, 9, 0);
} 

static int change_item(struct list_head *h)
{
assert (cur_item != NULL);
	if(h == &cur_submenu->items) return 0;
	cur_item = list_entry(h, struct item_t, l_submenu);
	return 1; 
}

static void change_submenu(struct list_head *h)
{
	if(h == &menu.submenus) return;
assert(cur_submenu != NULL);	
	mvwchgat(menu.wd, 0, cur_submenu->coord_x, 
		strlen(cur_submenu->title)+4, A_NORMAL, 9, 0);

	cur_submenu = list_entry(h, struct submenu_t, l_menu); 
	item_cursor = 0;
	if(submenu_wd) {
		cur_item = list_entry(cur_submenu->items.prev, struct item_t, l_submenu);
		delwin(submenu_wd);
		submenu_create(cur_submenu);
		submenu_print(cur_submenu);
		highlight_item(cur_submenu, 1);
	}	
	mvwchgat(menu.wd, 0, cur_submenu->coord_x, 
		strlen(cur_submenu->title)+4, A_REVERSE, 9, 0);
	wnoutrefresh(menu.wd);
//	submenu_refresh(cur_submenu);
redrawwin(main_win);
wnoutrefresh(info_win.wd);
redrawwin(info_win.wd);
}	

static bool submenu_show ()
{
	if(cur_item) return true;
	submenu_create(cur_submenu);
	submenu_print(cur_submenu);
	submenu_refresh(cur_submenu);	
	cur_item = list_entry(cur_submenu->items.prev, struct item_t, l_submenu);
	highlight_item(cur_submenu, 1);
	return false;
}


bool menu_keys(int key)
{
        if(key == KEY_F(9)) {
		if(!menu.wd) menu_create();
		else menu_destroy();
		return KEY_HANDLED;
	}
	if(!menu.wd) return KEY_SKIPPED;
	dolog("%s: submenu_wd %p\n", __FUNCTION__, submenu_wd);	
	switch(key) {
	case KEY_ESC:
		menu_destroy();
		break;
	case KEY_RIGHT:
		change_submenu(cur_submenu->l_menu.prev);
		break;
	case KEY_LEFT:
		change_submenu(cur_submenu->l_menu.next);
		break;
	case KEY_DOWN:
		if(!submenu_show()) return KEY_HANDLED; 
		if(change_item(cur_item->l_submenu.prev))
			highlight_item(cur_submenu, 1);
		dolog("%s: cur item %s\n", __FUNCTION__, cur_item->name);
		break;	
	case KEY_UP:
		if(!submenu_show()) return KEY_HANDLED; 
		if(change_item(cur_item->l_submenu.next))
			highlight_item(cur_submenu, -1);
		break;
	case KEY_ENTER:
		if(cur_item) {
			if(cur_item->hook) cur_item->hook();
			menu_destroy();
		}	
		break;
	case 'q':
		menu_destroy();
		break;
	default: 
		dolog("%s: [%d] skipped\n", __FUNCTION__, key);
		return KEY_HANDLED;
	}
	dolog("%s: [%d] accepted\n", __FUNCTION__, key);
	return KEY_HANDLED;
}

/* 
 * Called by resize() in main if SIGWINCH was delivered.
 */
void menu_resize(void)
{
	if(!menu.wd) return;
	set_size();
	wresize(menu.wd, 1, menu.cols);
	titles_print();
	menu_refresh();
	redrawwin(menu.wd);
}
