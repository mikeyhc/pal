#include <assert.h>
#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#define TIMEOUT 500

typedef struct {
	char name[30];
	char status[16];
} Ship;

typedef struct {
	unsigned debt_total;
	unsigned debt_repaid;
	unsigned overhaul_frontier;
	unsigned overhaul_standard;
	unsigned overhaul_advanced;
} Costs;

typedef enum {
	UI_DEFAULT,
	UI_BATTLE
} UIType;

void render_bar(char *name, int current, int total)
{
	int i;

	printw("%7s [", name);
	attron(A_REVERSE);
	for (i = 0; i < current * 2; i++)
		addch(' ');
	attroff(A_REVERSE);
	for (i = 0; i < (total - current) * 2; i++)
		addch(' ');
	printw("] (%d/%d)", current, total);
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

	if (!strcmp(vals[0], "ship.hull")) {
		move(2, 0);
		v = atoi(vals[1]);
		render_bar("Hull", v, v);
	} else if (!strcmp(vals[0], "ship.energy")) {
		move(3, 0);
		v = atoi(vals[1]);
		render_bar("Energy", v, v);
	} else if (!strcmp(vals[0], "ship.armor")) {
		move(4, 0);
		v = atoi(vals[1]);
		render_bar("Armor", v, v);
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
	int offset = 50;
	Costs costs;

	sqlite3_exec(db, "SELECT * FROM properties WHERE name LIKE 'debt.%' "
			"OR name LIKE 'overhaul.%'", &cost_info_callback,
			&costs, NULL);

	mvprintw(1, offset, "Ship Costs:");
	mvprintw(2, offset + 2, "Debt:");
	mvprintw(3, offset + 4, "Total:  %10d", costs.debt_total);
	mvprintw(4, offset + 4, "Repaid: %10d (%3d%)", costs.debt_repaid,
			costs.debt_repaid * 100 / costs.debt_total);
	mvprintw(5, offset + 4, "Repayments:");
	mvprintw(6, offset + 6, "Per Cycle:   %8.0f",
			round(costs.debt_total / 20.0));
	mvprintw(7, offset + 6, "Per Segment: %8.0f",
			round(costs.debt_total / 180.0));
	mvprintw(8, offset + 2, "Servicing:");
	mvprintw(9, offset + 4, "Frontier: %13d", costs.overhaul_frontier);
	mvprintw(10, offset + 4, "Standard: %13d", costs.overhaul_standard);
	mvprintw(11, offset + 4, "Advanced: %13d", costs.overhaul_advanced);
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
module_info_callback(void *vcounter, int cols, char **vals, char **names)
{
	int *counter = vcounter;
	if (*counter > 0 && *counter % 3 == 0)
		addch('\n');
	printw("%25s  [%s]", vals[0], vals[1]);
	(*counter)++;

	return 0;
}

int
module_info_header(void *vcounter, int cols, char **vals, char **names)
{
	int *counter = vcounter;
	int max_modules = atoi(vals[0]);

	mvprintw(13, 0, "Installed Modules (%d free):\n",
			max_modules - *counter);
	return 0;
}

void
module_info(sqlite3 *db)
{
	int counter = 0;

	move(14, 0);
	sqlite3_exec(db, "SELECT * FROM modules", &module_info_callback,
			&counter, NULL);
	sqlite3_exec(db, "SELECT value FROM properties "
			"WHERE name = 'ship.max_modules'", &module_info_header,
			&counter, NULL);
}

int
feature_info_callback(void *vcounter, int cols, char **vals, char **names)
{
	int *counter = vcounter;
	if (*counter > 0 && *counter % 3 == 0)
		addch('\n');
	printw("%25s  [%s]", vals[0], vals[1]);
	(*counter)++;

	return 0;
}

void
feature_info(sqlite3 *db)
{
	int counter = 0;

	mvprintw(19, 0, "Installed Features:\n");
	sqlite3_exec(db, "SELECT * FROM features", &feature_info_callback,
			&counter, NULL);
}

int
cargo_info_callback(void *vcounter, int cols, char **vals, char **names)
{
	int *counter = vcounter;
	if (*counter > 0 && *counter % 3 == 0)
		addch('\n');
	printw("%25s  [%2s]", vals[0], vals[1]);
	(*counter)++;

	return 0;
}

void
cargo_info(sqlite3 *db)
{
	int counter = 0;

	mvprintw(22, 0, "Cargo Contents:\n");
	sqlite3_exec(db, "SELECT * FROM cargo", &cargo_info_callback,
			&counter, NULL);
}

void
error_info()
{
	mvprintw(LINES - 2, 0,
			"+++ ERROR +++ I AM COLD AND ALONE +++ ERROR +++");
}

void
load_default_ui(sqlite3 *db)
{
	move(0, 0);
	ship_info(db);
	crew_info(db);
	cost_info(db);
	module_info(db);
	feature_info(db);
	cargo_info(db);
	error_info();

	refresh();
}

void
load_nav_ui()
{
	move(LINES - 1, 0);
	printw(" F1 ");
	attron(A_REVERSE);
	printw(" Ship ");
	attroff(A_REVERSE);

}

int
main(int argc, char **argv)
{
	sqlite3 *db;
	struct pollfd ufds[1];
	int running = 1, ready;
	char c;
	UIType ui;

	setlocale(LC_ALL, "");
	assert(!sqlite3_open("pal.db", &db));

	ufds[0].fd = STDIN_FILENO;
	ufds[0].events = POLLIN;

	initscr();
	cbreak();
	keypad(stdscr, 1);

	load_default_ui(db);
	load_nav_ui();
	while (1) {
		assert(poll(ufds, 1, TIMEOUT) >= 0);
		if (ufds[0].revents & POLLIN) {
			read(STDIN_FILENO, &c, 1);
			mvprintw(LINES - 2, 0, "got char %c\n", c);
			switch (c) {
			case 'q':
				goto END;
				break;
			case 'b':
				ui = UI_BATTLE;
				break;
			case 's':
				ui = UI_DEFAULT;
				break;
			}
		}

		switch (ui) {
		case UI_DEFAULT:
			load_default_ui(db);
			break;
		case UI_BATTLE:
			/* TODO: create a battle UI */
			load_default_ui(db);
			break;
		}
		load_nav_ui();
	}

END:
	endwin();
	sqlite3_close(db);
	return 0;
}
