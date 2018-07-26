#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#define TIMEOUT 500
#define OFFSET  50
#define MAX_ENTRIES 10
#define ENTRY_LENGTH 30

typedef struct {
	int max, current;
} Bar;

typedef struct {
	char name[30];
	char status[16];
	Bar hull, energy;
} Ship;

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
	UI_BATTLE
} UIType;

typedef struct {
	int counters[2];
	int x, y;
} TableBuilder;

void render_bar(char *name, Bar *bar)
{
	int i;

	printw("%7s [", name);
	attron(A_REVERSE);
	for (i = 0; i < bar->current * 2; i++)
		addch(' ');
	attroff(A_REVERSE);
	for (i = 0; i < (bar->max- bar->current) * 2; i++)
		addch(' ');
	printw("] (%d/%d)", bar->current, bar->max);
}

void
ship_name_render(Ship *ship)
{
	if (ship->name[0] == '\0' || ship->status[0] == '\0')
		return;
	mvprintw(0, 0, "%s  [%s]", ship->name, ship->status);
}

int
ship_info_callback(void *vship, int cols,  char **vals, char **names)
{
	int v;
	Ship *ship = vship;
	Bar b;

	if (!strcmp(vals[0], "ship.hull")) {
		v = atoi(vals[1]);
		ship->hull.max = v;
		if (ship->hull.max >= 0 && ship->hull.current >= 0) {
			move(2, 0);
			render_bar("Hull", &ship->hull);
		}
	} else if (!strcmp(vals[0], "ship.current_hull")) {
		v = atoi(vals[1]);
		ship->hull.current = v;
		if (ship->hull.max >= 0 && ship->hull.current >= 0) {
			move(2, 0);
			render_bar("Hull", &ship->hull);
		}
	} else if (!strcmp(vals[0], "ship.energy")) {
		v = atoi(vals[1]);
		ship->energy.max = v;
		if (ship->energy.max >= 0 && ship->energy.current >= 0) {
			move(3, 0);
			render_bar("Hull", &ship->energy);
		}
	} else if (!strcmp(vals[0], "ship.current_energy")) {
		v = atoi(vals[1]);
		ship->energy.current = v;
		if (ship->energy.max >= 0 && &ship->energy.current >= 0) {
			move(3, 0);
			render_bar("Hull", &ship->energy);
		}
	} else if (!strcmp(vals[0], "ship.armor")) {
		move(4, 0);
		v = atoi(vals[1]);
		b.current = b.max = v;
		render_bar("Armor", &b);
	} else if (!strcmp(vals[0], "ship.name")) {
		strncpy(ship->name, vals[1], 29);
		ship_name_render(ship);
	} else if (!strcmp(vals[0], "ship.status")) {
		strncpy(ship->status, vals[1], 15);
		ship_name_render(ship);
	}
	return 0;
}

void
ship_info(sqlite3 *db)
{
	int i;
	Ship ship;

	ship.name[0] = '\0';
	ship.status[0] = '\0';
	ship.hull.max = ship.hull.current = -1;
	ship.energy.max = ship.energy.current = -1;
	sqlite3_exec(db, "SELECT * FROM properties WHERE name LIKE 'ship.%'",
			&ship_info_callback, &ship, NULL);
}

int
cost_info_callback(void *vcosts, int cols, char **vals, char **names)
{
	unsigned v;
	Costs *costs = vcosts;

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
cost_info(sqlite3 *db)
{
	Costs costs;
	int repayment_percent;
	float repayment_points;
	char *msg = "Repaid";

	sqlite3_exec(db, "SELECT * FROM properties WHERE name LIKE 'debt.%' "
			"OR name LIKE 'overhaul.%'", &cost_info_callback,
			&costs, NULL);

	mvprintw(1, OFFSET, "Ship Costs:");
	mvprintw(2, OFFSET + 2, "Debt:");
	mvprintw(3, OFFSET + 4, "Total:  %10d", costs.debt_total);
	repayment_percent = costs.debt_repaid * 100 / costs.debt_total;
	mvprintw(4, OFFSET + 4, "Repaid: %10d (%3d%)", costs.debt_repaid,
			repayment_percent);
	mvprintw(5, OFFSET + 4, "Repayments:");
	mvprintw(6, OFFSET + 6, "Per Cycle:   %8.0f",
			round(costs.debt_total / 20.0));
	mvprintw(7, OFFSET + 6, "Per Segment: %8.0f",
			round(costs.debt_total / 180.0));
	mvprintw(8, OFFSET + 2, "Servicing:");
	mvprintw(9, OFFSET + 4, "Frontier: %13d", costs.overhaul_frontier);
	mvprintw(10, OFFSET + 4, "Standard: %13d", costs.overhaul_standard);
	mvprintw(11, OFFSET + 4, "Advanced: %13d", costs.overhaul_advanced);

	repayment_points = repayment_percent / (100.0 / (LINES - 3));
	mvprintw(LINES - 3, COLS - strlen(msg), msg);
	attron(A_REVERSE);
	mvvline(LINES - 3 - repayment_points, COLS - 1, ' ',
			(int)round(repayment_points));
	attroff(A_REVERSE);
}

int
crew_info_callback(void *unused, int cols, char **vals, char **names)
{
	int y;

	printw("%22s", vals[0]);
	y = getcury(stdscr);
	mvprintw(y, 24, "[%s]\n", vals[1]);

}

void
crew_info(sqlite3 *db)
{
	int i;

	mvprintw(6, 0, "Crew Status:\n");
	sqlite3_exec(db, "SELECT * FROM crew", &crew_info_callback, NULL,
			NULL);
}

int
module_info_callback(void *vbuilder, int cols, char **vals, char **names)
{
	TableBuilder *builder = vbuilder;
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	if (!strcmp(vals[1], "EE"))
		attron(COLOR_PAIR(1));
	else
		builder->counters[1]++;
	mvprintw(builder->y, builder->x, "%25s  [%s]", vals[0], vals[1]);
	builder->x += 31;
	if (!strcmp(vals[1], "EE"))
		attroff(COLOR_PAIR(1));
	builder->counters[0]++;

	return 0;
}

int
module_info_header(void *vcounter, int cols, char **vals, char **names)
{
	int *counters = vcounter;
	int max_modules = atoi(vals[0]);

	mvprintw(13, 0, "Installed Modules (%d free, %d enabled):",
			max_modules - *counters, counters[1]);
	return 0;
}

void
module_info(sqlite3 *db)
{
	TableBuilder builder = { {0, 0}, 0, 14 };

	sqlite3_exec(db, "SELECT * FROM modules", &module_info_callback,
			&builder, NULL);
	sqlite3_exec(db, "SELECT value FROM properties "
			"WHERE name = 'ship.max_modules'", &module_info_header,
			&builder, NULL);
}

int
feature_info_callback(void *vbuilder, int cols, char **vals, char **names)
{
	TableBuilder *builder= vbuilder;
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	mvprintw(builder->y, builder->x, "%25s  [%s]", vals[0], vals[1]);
	builder->x += 36;
	builder->counters[0]++;

	return 0;
}

void
feature_info(sqlite3 *db)
{
	TableBuilder builder = { {0}, 0, 20 };

	mvprintw(19, 0, "Installed Features:");
	sqlite3_exec(db, "SELECT * FROM features", &feature_info_callback,
			&builder, NULL);
}

int
cargo_info_callback(void *vbuilder, int cols, char **vals, char **names)
{
	TableBuilder *builder = vbuilder;
	if (builder->counters[0] > 0 && builder->counters[0] % 3 == 0) {
		builder->y++;
		builder->x = 0;
	}
	mvprintw(builder->y, builder->x, "%25s  [%2s]", vals[0], vals[1]);
	builder->x += 36;
	builder->counters[0]++;

	return 0;
}

void
cargo_info(sqlite3 *db)
{
	TableBuilder builder = { {0}, 0, 23 };

	mvprintw(22, 0, "Cargo Contents:");
	sqlite3_exec(db, "SELECT * FROM cargo", &cargo_info_callback,
			&builder, NULL);
}

void
error_info()
{
	int i;
	char *msg = "+++ ERROR +++ I have accidently seen spoilers for "
		"the next episode ;-; +++ ERROR +++";
	attron(A_REVERSE);
	mvprintw(LINES - 2, 0, msg);
	// TODO: this is terrible, think of something better
	for (i = strlen(msg); i < COLS; i++)
		addch(' ');
	attron(A_REVERSE);
}

void
render_menu_selected(int y, int x, int i, MenuInfo *menuinfo)
{
	if (menuinfo->mode == MM_EDIT) {
		mvprintw(y, x, menuinfo->entries[i]);
		attron(A_REVERSE);
		addch(' ');
		attroff(A_REVERSE);
	} else {
		attron(A_REVERSE);
		mvprintw(y, x, menuinfo->entries[i]);
		hline(' ', ENTRY_LENGTH - strlen(menuinfo->entries[i]));
		attroff(A_REVERSE);
	}
}

void
render_menu(int y, int x, MenuInfo *menuinfo)
{
	int i;

	for (i = 0; i < MAX_ENTRIES; i++) {
		if (i == menuinfo->curline)
			render_menu_selected(y + i, x, i, menuinfo);
		else
			mvprintw(y + i, x, menuinfo->entries[i]);
	}
}

void
battle_info(MenuInfo *menuinfo)
{
	mvprintw(1, OFFSET, "Combat Order:");
	render_menu(2, OFFSET + 2, menuinfo);
}

void
load_default_ui(sqlite3 *db)
{
	erase();
	move(0, 0);
	ship_info(db);
	crew_info(db);
	cost_info(db);
	module_info(db);
	feature_info(db);
	cargo_info(db);
	error_info();
}

void
load_battle_ui(sqlite3 *db, MenuInfo *menuinfo)
{
	erase();
	move(0, 0);
	ship_info(db);
	crew_info(db);
	battle_info(menuinfo);
	module_info(db);
	feature_info(db);
	cargo_info(db);
	error_info();
}

void
load_nav_ui(UIType ui)
{
	move(LINES - 1, 0);
	attron(A_REVERSE);
	printw(" F1 ");
	if (ui != UI_DEFAULT)
		attroff(A_REVERSE);
	printw(" Ship ");
	if (ui == UI_DEFAULT)
		attroff(A_REVERSE);
	addch(' ');
	attron(A_REVERSE);
	printw(" F2 ");
	if (ui != UI_BATTLE)
		attroff(A_REVERSE);
	printw(" Battle ");
	if (ui == UI_BATTLE)
		attroff(A_REVERSE);
	addch(' ');
	attron(A_REVERSE);
	printw(" F10 ");
	attroff(A_REVERSE);
	printw(" Quit ");
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
main(int argc, char **argv)
{
	sqlite3 *db;
	struct pollfd ufds[1];
	MenuInfo menuinfo;
	int running = 1, ready;
	int c, failcount = 0;
	UIType ui = UI_DEFAULT;

	setlocale(LC_ALL, "");
	assert(!sqlite3_open("pal.db", &db));

	menuinfo_initialize(&menuinfo);

	ufds[0].fd = STDIN_FILENO;
	ufds[0].events = POLLIN;

	initscr();
	cbreak();
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	keypad(stdscr, 1);

	load_default_ui(db);
	load_nav_ui(ui);
	refresh();
	while (1) {
		if (poll(ufds, 1, TIMEOUT) < 0) {
			if (failcount == 5)
				assert(0);
			continue;
		}
		failcount = 0;
		if (ufds[0].revents & POLLIN) {
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
			default:
				if (ui == UI_BATTLE)
					handle_key(c, &menuinfo);
				break;
			}
		}

		switch (ui) {
		case UI_DEFAULT:
			load_default_ui(db);
			break;
		case UI_BATTLE:
			load_battle_ui(db, &menuinfo);
			break;
		}
		load_nav_ui(ui);
		refresh();
	}

END:
	endwin();
	sqlite3_close(db);
	return 0;
}
