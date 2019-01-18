#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include <ncurses.h>
#include <zmq.h>

#define MAX_OPTION_LEN 32
#define MAX_WINDOWS 1
#define TOTAL_OPTIONS 2
#define SUBMENU_WIDTH 20

typedef enum {
	FOCUS_SIDEMENU,
} Focus;

typedef struct {
	unsigned pos;
	char options[TOTAL_OPTIONS][MAX_OPTION_LEN];
	void (*option_render[TOTAL_OPTIONS])(void);
} SideMenu;

static void
render_sidemenu(WINDOW *w, SideMenu *menu)
{
	unsigned i;

	werase(w);
	box(w, '|', '-');
	for (i = 0; i < TOTAL_OPTIONS; i++) {
		if (i == menu->pos)
			wattron(w, A_REVERSE);
		mvwprintw(w, 1 + i, 1, menu->options[i]);
		if (i == menu->pos) {
			whline(w, ' ', SUBMENU_WIDTH - 2
					- strlen(menu->options[i]));
			wattroff(w, A_REVERSE);
		}
	}
	wrefresh(w);
}

static void
render_ship_config(void)
{
}


static void
render_crew_config(void)
{
}

static void
initialize_sidemenu(SideMenu *menu)
{
	menu->pos = 0;
	strncpy(menu->options[0], "Ship Config", MAX_OPTION_LEN);
	menu->option_render[0] = &render_ship_config;
	strncpy(menu->options[1], "Crew Config", MAX_OPTION_LEN);
	menu->option_render[1] = &render_crew_config;
}

int
main(void)
{
	void *context, *client;
	WINDOW *windows[MAX_WINDOWS];
	SideMenu menu;
	Focus focus = FOCUS_SIDEMENU;

	initialize_sidemenu(&menu);

	context = zmq_ctx_new();
	client = zmq_socket(context, ZMQ_REQ);

	setlocale(LC_ALL, "");
	assert(!zmq_connect(client, "tcp://localhost:5555"));

	initscr();
	cbreak();
	start_color();
	keypad(stdscr, 1);
	curs_set(0);
	noecho();

	windows[0] = newwin(LINES, SUBMENU_WIDTH, 0, 0);

	clear();
	refresh();

	render_sidemenu(windows[0], &menu);

	while (1) {
		switch (getch()) {
		case KEY_UP:
			if (focus == FOCUS_SIDEMENU) {
				menu.pos = (menu.pos + 1) % TOTAL_OPTIONS;
				render_sidemenu(windows[0], &menu);
			}
			break;
		case KEY_DOWN:
			if (focus == FOCUS_SIDEMENU) {
				if (menu.pos == 0)
					menu.pos = TOTAL_OPTIONS - 1;
				else
					menu.pos = menu.pos - 1;
				render_sidemenu(windows[0], &menu);
			}
			break;
		case KEY_F(10):
			goto END;
			break;
		}
	}

END:
	endwin();
	zmq_close(client);
	zmq_ctx_destroy(context);

	return 0;
}
