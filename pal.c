#include <assert.h>
#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

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
	mvprintw(LINES - 1, 0,
			"+++ ERROR +++ I AM COLD AND ALONE +++ ERROR +++");
}

void
load_ui(sqlite3 *db)
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

int
main(int argc, char **argv)
{
	sqlite3 *db;

	setlocale(LC_ALL, "");
	assert(!sqlite3_open("pal.db", &db));
	initscr();
	cbreak();
	keypad(stdscr, 1);

	load_ui(db);
	while (getch() != 'q')
		load_ui(db);

	endwin();
	sqlite3_close(db);
	return 0;
}
