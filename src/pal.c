#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>
#include <zmq.h>

#include "pal.h"

#define TIMEOUT 2000
#define OFFSET  50
#define MAX_ENTRIES 10
#define ENTRY_LENGTH 30

#define MAX_WINDOWS 9

#define zmq_poll_init(item, s, f, e)		\
	item.socket = s;			\
	item.fd = f;				\
	item.events = e;			\
	item.revents = 0;

typedef struct {
	Ship ship;
	WINDOW *window;
} ShipInfoRender;

typedef struct {
	unsigned debt_total;
	unsigned debt_repaid;
	unsigned overhaul_frontier;
	unsigned overhaul_standard;
	unsigned overhaul_advanced;
} Costs;

typedef enum {
	MM_VIEW,
	MM_EDIT
} MenuMode;

typedef struct {
	int curline, curcol;
	MenuMode mode;
	char entries[MAX_ENTRIES][ENTRY_LENGTH];
} MenuInfo;

typedef enum {
	UI_DEFAULT,
	UI_BATTLE,
	UI_ACCOUNTS
} UIType;

typedef struct {
	int counters[2];
	int x, y;
} TableBuilder;

typedef struct {
	TableBuilder table;
	WINDOW *window;
} TableRender;

void
render_bar(char *name, WINDOW *w, Bar *bar)
{
	int i;

	wprintw(w, "%13s [", name);
	wattron(w, A_REVERSE);
	for (i = 0; i < bar->current * 2; i++)
		waddch(w, ' ');
	wattroff(w, A_REVERSE);
	for (i = 0; i < (bar->max- bar->current) * 2; i++)
		waddch(w, ' ');
	wprintw(w, "] (%d/%d)", bar->current, bar->max);
}

void
render_value(WINDOW *w, char *name, char *value)
{
	wprintw(w, "%13s %s", name, value);
}

void
ship_name_render(WINDOW *w, Ship *ship)
{
	if (ship->name[0] == '\0' || ship->status[0] == '\0')
		return;
	mvwprintw(w, 0, 0, "%s  [", ship->name);
	if (!strcmp(ship->status, "IN COMBAT")
			|| !strcmp(ship->status, "CRASHED"))
		wattron(w, A_BLINK);
	wprintw(w, ship->status);
	if (!strcmp(ship->status, "IN COMBAT")
			|| !strcmp(ship->status, "CRASHED"))
		wattroff(w, A_BLINK);
	wprintw(w, "]");
}

static void
render_ship(char *buffer, WINDOW *w)
{
	Ship ship;
	int pos = 0;

	strncpy(ship.name, buffer, SHIPNAME_LEN);
	pos += SHIPNAME_LEN;
	strncpy(ship.status, buffer + pos, SHIPSTATUS_LEN);
	pos += SHIPSTATUS_LEN;
	ship.hull.current = buffer[pos++];
	ship.hull.max = buffer[pos++];
	ship.energy.current = buffer[pos++];
	ship.energy.max = buffer[pos++];
	ship.armor.current = buffer[pos++];
	ship.armor.max = buffer[pos++];

	werase(w);
	ship_name_render(w, &ship);
	wmove(w, 2, 0);
	render_bar("Hull", w, &ship.hull);
	wmove(w, 3, 0);
	render_bar("Energy", w, &ship.energy);
	wmove(w, 4, 0);
	render_bar("Armor", w, &ship.armor);
	wrefresh(w);
}

int
cost_info_callback(void *vcosts, int cols, char **vals, char **names)
{
	unsigned v;
	Costs *costs = vcosts;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	v = atoi(vals[1]);
	if (!strcmp(vals[0], "debt.total")) {
		costs->debt_total = v;
	} else if (!strcmp(vals[0], "debt.repaid")) {
		costs->debt_repaid = v;
	} else if (!strcmp(vals[0], "overhaul.frontier")) {
		costs->overhaul_frontier = v;
	} else if (!strcmp(vals[0], "overhaul.standard")) {
		costs->overhaul_standard = v;
	} else if (!strcmp(vals[0], "overhaul.advanced")) {
		costs->overhaul_advanced = v;
	}

	return 0;
}

void
cost_info(sqlite3 *db, WINDOW *w, WINDOW *bw)
{
	Costs costs;
	int repayment_percent, lines, cols;
	float repayment_points;
	char *msg = "Repaid";

	sqlite3_exec(db, "SELECT * FROM properties WHERE name LIKE 'debt.%' "
			"OR name LIKE 'overhaul.%'", &cost_info_callback,
			&costs, NULL);

	mvwprintw(w, 0, 0, "Ship Costs:");
	mvwprintw(w, 1, 2, "Debt:");
	mvwprintw(w, 2, 4, "Total:  %10d", costs.debt_total);
	if (costs.debt_total > 0)
		repayment_percent = costs.debt_repaid * 100 / costs.debt_total;
	if (costs.debt_repaid > 0 && repayment_percent == 0)
		repayment_percent = 1;
	mvwprintw(w, 3, 4, "Repaid: %10d (%3d%)", costs.debt_repaid,
			repayment_percent);
	mvwprintw(w, 4,  4, "Repayments:");
	mvwprintw(w, 5,  6, "Per Cycle:   %8.0f",
			round(costs.debt_total / 20.0));
	mvwprintw(w, 6, 6, "Per Segment: %8.0f",
			round(costs.debt_total / 180.0));
	mvwprintw(w, 7, 2, "Servicing:");
	mvwprintw(w, 8, 4, "Frontier: %13d", costs.overhaul_frontier);
	mvwprintw(w, 9, 4, "Standard: %13d", costs.overhaul_standard);
	mvwprintw(w, 10, 4, "Advanced: %13d", costs.overhaul_advanced);

	getmaxyx(bw, lines, cols);
	repayment_points = repayment_percent / (100.0 / (lines - 1));
	if (repayment_percent > 0 && repayment_points < 1)
		repayment_points = 1.0;
	mvwprintw(bw, lines - 1, 1, msg);
	mvwprintw(bw, lines - 2, 3, "0%%");
	mvwprintw(bw, 0, 1, "100%%");
	wattron(bw, A_REVERSE);
	mvwvline(bw, lines - repayment_points - 1, cols - 1, ' ',
			(int)round(repayment_points));
	wattroff(bw, A_REVERSE);
	wrefresh(bw);
}

int
crew_info_callback(void *vw, int cols, char **vals, char **names)
{
	int y;
	WINDOW *w = vw;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	y = getcury(w);
	mvwprintw(w, y + 1, 0, "%22s", vals[0]);
	mvwprintw(w, y + 1, 24, "[%s]", vals[1]);

	return 0;
}

void
crew_info(sqlite3 *db, WINDOW *w)
{
	mvwprintw(w, 0, 0, "Crew Status:\n");
	sqlite3_exec(db, "SELECT * FROM crew", &crew_info_callback, w,
			NULL);
}

int
module_info_callback(void *vrender, int cols, char **vals, char **names)
{
	TableRender *render = vrender;
	WINDOW *w = render->window;
	TableBuilder *builder = &render->table;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	if (!strcmp(vals[1], "EE"))
		wattron(w, COLOR_PAIR(1));
	else
		builder->counters[1]++;
	mvwprintw(w, builder->y, builder->x, "%25s  [%2s]", vals[0], vals[1]);
	builder->x += 36;
	if (!strcmp(vals[1], "EE"))
		wattroff(w, COLOR_PAIR(1));
	builder->counters[0]++;

	return 0;
}

int
module_info_header(void *vrender, int cols, char **vals, char **names)
{
	TableRender *render = vrender;
	int *counters = render->table.counters;
	int max_modules = atoi(vals[0]);

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	mvwprintw(render->window, 0, 0,
			"Installed Modules (%d free, %d enabled):",
			max_modules - *counters, counters[1]);
	return 0;
}

void
module_info(sqlite3 *db, WINDOW *w)
{
	TableRender render = {
		{ {0, 0}, 0, 1 },
		w
	};


	sqlite3_exec(db, "SELECT * FROM modules", &module_info_callback,
			&render, NULL);
	sqlite3_exec(db, "SELECT value FROM properties "
			"WHERE name = 'ship.max_modules'", &module_info_header,
			&render, NULL);
}

int
feature_info_callback(void *vrender, int cols, char **vals, char **names)
{
	TableRender *render = vrender;
	TableBuilder *builder = &render->table;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	mvwprintw(render->window, builder->y, builder->x,
			"%25s  [%s]", vals[0], vals[1]);
	builder->x += 36;
	builder->counters[0]++;

	return 0;
}

void
feature_info(sqlite3 *db, WINDOW *w)
{
	TableRender render = {
		{ {0}, 0, 1 },
		w
	};

	mvwprintw(w, 0, 0, "Installed Features:");
	sqlite3_exec(db, "SELECT * FROM features", &feature_info_callback,
			&render, NULL);
}

int
cargo_info_callback(void *vrender, int cols, char **vals, char **names)
{
	TableRender *render = vrender;
	TableBuilder *builder = &render->table;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	mvwprintw(render->window, builder->y, builder->x,
			"%25s  [%2s]", vals[0], vals[1]);
	builder->x += 36;
	builder->counters[0]++;

	return 0;
}

void
cargo_info(sqlite3 *db, WINDOW *w)
{
	TableRender render = {
		{ {0}, 0, 1 },
		w,
	};

	mvwprintw(w, 0, 0, "Cargo Contents:");
	sqlite3_exec(db, "SELECT * FROM cargo", &cargo_info_callback,
			&render, NULL);
}

void
error_info(WINDOW *w)
{
	int maxx;
	char *msg = "+++ ERROR +++ I have accidently seen spoilers for "
		"the next episode ;-; +++ ERROR +++";
	wattron(w, A_REVERSE);
	mvwprintw(w, 0, 0, msg);
	maxx = getmaxx(w);
	whline(w, ' ', maxx - strlen(msg));
	wattroff(w, A_REVERSE);
}

void
render_menu_selected(WINDOW *w, int y, int x, int i, MenuInfo *menuinfo)
{
	if (menuinfo->mode == MM_EDIT) {
		mvwprintw(w, y, x, menuinfo->entries[i]);
		wattron(w, A_REVERSE);
		waddch(w,' ');
		wattroff(w, A_REVERSE);
	} else {
		wattron(w, A_REVERSE);
		mvwprintw(w, y, x, menuinfo->entries[i]);
		whline(w,' ', ENTRY_LENGTH - strlen(menuinfo->entries[i]));
		wattroff(w, A_REVERSE);
	}
}

void
render_menu(WINDOW *w, int y, int x, MenuInfo *menuinfo)
{
	int i;

	for (i = 0; i < MAX_ENTRIES; i++) {
		if (i == menuinfo->curline)
			render_menu_selected(w, y + i, x, i, menuinfo);
		else
			mvwprintw(w, y + i, x, menuinfo->entries[i]);
	}
}

void
battle_info(MenuInfo *menuinfo, WINDOW *w)
{
	mvwprintw(w, 0, 0, "Combat Order:");
	render_menu(w, 1, 2, menuinfo);
}

void
load_default_ui(sqlite3 *db, WINDOW **windows)
{
	crew_info(db, windows[1]);
	cost_info(db, windows[2], windows[8]);
	module_info(db, windows[3]);
	feature_info(db, windows[4]);
	cargo_info(db, windows[5]);
	error_info(windows[6]);
}

void
load_battle_ui(sqlite3 *db, WINDOW **windows, MenuInfo *menuinfo)
{
	crew_info(db, windows[1]);
	battle_info(menuinfo, windows[2]);
	module_info(db, windows[3]);
	feature_info(db, windows[4]);
	cargo_info(db, windows[5]);
	error_info(windows[6]);
}

void
load_accounts_ui(sqlite3 *db, WINDOW **windows)
{
	int i;

	for (i = 1; i < MAX_WINDOWS; i++)
		werase(windows[i]);
	crew_info(db, windows[1]);
	/* account info goes here */
	module_info(db, windows[3]);
	feature_info(db, windows[4]);
	cargo_info(db, windows[5]);
	error_info(windows[6]);
}

void
load_nav_ui(UIType ui, WINDOW *w)
{
	wmove(w, 0, 0);

	wattron(w, A_REVERSE);
	wprintw(w, " F1 ");
	if (ui != UI_DEFAULT)
		wattroff(w, A_REVERSE);
	wprintw(w, " Ship ");
	if (ui == UI_DEFAULT)
		wattroff(w, A_REVERSE);
	waddch(w, ' ');

	wattron(w, A_REVERSE);
	wprintw(w, " F2 ");
	if (ui != UI_BATTLE)
		wattroff(w, A_REVERSE);
	wprintw(w, " Battle ");
	if (ui == UI_BATTLE)
		wattroff(w, A_REVERSE);
	waddch(w, ' ');

	wattron(w, A_REVERSE);
	wprintw(w, " F3 ");
	if (ui != UI_ACCOUNTS)
		wattroff(w, A_REVERSE);
	wprintw(w, " Accounts ");
	if (ui == UI_ACCOUNTS)
		wattroff(w, A_REVERSE);

	waddch(w, ' ');
	wattron(w, A_REVERSE);
	wprintw(w, " F10 ");
	wattroff(w, A_REVERSE);
	wprintw(w, " Quit ");
}

void
menu_update_entry(MenuInfo *menuinfo, char c)
{
	if (c == 127 && menuinfo->curcol > 0) {
		menuinfo->curcol--;
		menuinfo->entries[menuinfo->curline][menuinfo->curcol] = '\0';
		return;
	}
	if (!isalnum(c) && c != ' ')
		return;
	if (menuinfo->curcol == ENTRY_LENGTH - 1)
		return;
	menuinfo->entries[menuinfo->curline][menuinfo->curcol] = c;
	menuinfo->curcol++;
	menuinfo->entries[menuinfo->curline][menuinfo->curcol] = '\0';
}

void
menu_move_entry(MenuInfo *menuinfo, int mov)
{
	char temp[ENTRY_LENGTH];
	int oldline = menuinfo->curline;
	int newline = menuinfo->curline + mov % MAX_ENTRIES;
	if (newline < 0)
		newline = MAX_ENTRIES - 1;
	strcpy(temp, menuinfo->entries[oldline]);
	strcpy(menuinfo->entries[oldline], menuinfo->entries[newline]);
	strcpy(menuinfo->entries[newline], temp);
	menuinfo->curline = newline;

}

void
menuinfo_initialize(MenuInfo *menuinfo)
{
	int i;

	menuinfo->curline = 0;
	menuinfo->curcol = 0;
	menuinfo->mode = MM_VIEW;
	for (i = 0; i < MAX_ENTRIES; i++)
		menuinfo->entries[i][0] = '\0';
}

void
handle_key(int c, MenuInfo *menuinfo)
{
	if (menuinfo->mode == MM_EDIT) {
		if (c == 10)
			menuinfo->mode = MM_VIEW;
		else
			menu_update_entry(menuinfo, c);
		return;
	}

	switch (c) {
	case 'j':
		menuinfo->curline = (menuinfo->curline + 1) % MAX_ENTRIES;
		menuinfo->curcol =
			strlen(menuinfo->entries[menuinfo->curline]);
		break;
	case 'k':
		if (menuinfo->curline == 0)
			menuinfo->curline = MAX_ENTRIES - 1;
		else
			menuinfo->curline = menuinfo->curline - 1;
		menuinfo->curcol =
			strlen(menuinfo->entries[menuinfo->curline]);
		break;
	case 'J':
		menu_move_entry(menuinfo, 1);
		break;
	case 'K':
		menu_move_entry(menuinfo, -1);
		break;
	case 'C':
		menuinfo_initialize(menuinfo);
		break;
	case 10:
		menuinfo->mode = MM_EDIT;
		break;
	}
}

int
main(void)
{
	void *context;
	void *subscriber;
	char buffer[SHIP_MESSAGE_LEN];
	sqlite3 *db;
	zmq_pollitem_t items[2];
	MenuInfo menuinfo;
	int i, c, failcount = 0, msgcount = 0;
	UIType ui = UI_DEFAULT;
	int window_positions[MAX_WINDOWS][4] = {
		{ 6, 40, 0, 0 },
		{ 8, 40, 7, 0 },
		{ 20, 40, 0, OFFSET },
		{ 6, 120, 15, 0 },
		{ 3, 120, 21, 0 },
		{ 6, 120, 24, 0 },
		{ 1, -1, -3, 0 },
		{ 1, -1, -2, 0 },
		{ -3, 7, 0, -8},
	};
	WINDOW *windows[MAX_WINDOWS];

	setlocale(LC_ALL, "");
	assert(!sqlite3_open("pal.db", &db));
	context = zmq_ctx_new();
	subscriber = zmq_socket(context, ZMQ_SUB);
	assert(!zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0));
	assert(!zmq_connect(subscriber, "tcp://localhost:5556"));
	zmq_poll_init(items[0], 0, STDIN_FILENO, ZMQ_POLLIN);
	zmq_poll_init(items[1], subscriber, 0, ZMQ_POLLIN);

	menuinfo_initialize(&menuinfo);

	initscr();
	cbreak();
	noecho();
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	keypad(stdscr, 1);
	curs_set(0);

	for (i = 0; i < MAX_WINDOWS; i++) {
		if (window_positions[i][0] < 0)
			window_positions[i][0] = LINES + 1 +
				window_positions[i][0];
		if (window_positions[i][1] < 0)
			window_positions[i][1] = COLS + 1 +
				window_positions[i][1];
		if (window_positions[i][2] < 0)
			window_positions[i][2] = LINES + 1 +
				window_positions[i][2];
		if (window_positions[i][3] < 0)
			window_positions[i][3] = COLS + 1 +
				window_positions[i][3];
		windows[i] = newwin(
				window_positions[i][0],
				window_positions[i][1],
				window_positions[i][2],
				window_positions[i][3]);
	}

	while (1) {
		if (zmq_poll(items, 2, TIMEOUT) < 0) {
			if (failcount == 5)
				assert(0);
			continue;
		}
		failcount = 0;

		if (items[1].revents & ZMQ_POLLIN) {
			zmq_recv(subscriber, buffer, SHIP_MESSAGE_LEN, 0);
			switch (buffer[0]) {
			case SHIP_UPDATE:
				wattron(windows[0], COLOR_PAIR(2));
				render_ship(buffer + 1, windows[0]);
				wattroff(windows[0], COLOR_PAIR(2));
				break;
			}
			msgcount = (msgcount + 1) % 100;
		}

		if (items[0].revents & ZMQ_POLLIN) {
			c =  getch();
			switch (c) {
			case KEY_F(10):
				goto END;
				break;
			case KEY_F(1):
				ui = UI_DEFAULT;
				break;
			case KEY_F(2):
				ui = UI_BATTLE;
				break;
			case KEY_F(3):
				ui = UI_ACCOUNTS;
				break;
			default:
				if (ui == UI_BATTLE)
					handle_key(c, &menuinfo);
				break;
			}
		}

		for (i = 0; i < MAX_WINDOWS; i++) {
			werase(windows[i]);
			wattron(windows[i], COLOR_PAIR(2));
		}
		switch (ui) {
		case UI_DEFAULT:
			load_default_ui(db, windows);
			break;
		case UI_BATTLE:
			load_battle_ui(db, windows, &menuinfo);
			break;
		case UI_ACCOUNTS:
			load_accounts_ui(db, windows);
			break;
		}
		load_nav_ui(ui, windows[7]);
		for (i = 1; i < MAX_WINDOWS; i++) {
			wattroff(windows[i], COLOR_PAIR(2));
			wrefresh(windows[i]);
		}
	}

END:
	endwin();
	sqlite3_close(db);
	zmq_close(subscriber);
	zmq_ctx_destroy(context);
	return 0;
}
