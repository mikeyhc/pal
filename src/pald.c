#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>
#include <zmq.h>

#include "pal.h"

#define BUFSIZE 256
#define QUERY_LENGTH 128

typedef struct {
	Ship ship;
	FinanceEntry finances[FINANCE_ENTRY_COUNT];
	int finance_count;
} GameState;

typedef struct {
	void *data;
	int *index;
} IndexedState;

static int
ship_info_callback(void *vship, int cols,  char **vals, char **names)
{
	Ship *ship = vship;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */
	if (!strcmp(vals[0], "ship.hull")) {
		ship->hull.max = atoi(vals[1]);
	} else if (!strcmp(vals[0], "ship.current_hull")) {
		ship->hull.current = atoi(vals[1]);
	} else if (!strcmp(vals[0], "ship.energy")) {
		ship->energy.max = atoi(vals[1]);
	} else if (!strcmp(vals[0], "ship.current_energy")) {
		ship->energy.current = atoi(vals[1]);
	} else if (!strcmp(vals[0], "ship.armor")) {
		ship->armor.current = atoi(vals[1]);
		ship->armor.max = ship->armor.current;
	} else if (!strcmp(vals[0], "ship.name")) {
		strncpy(ship->name, vals[1], 29);
	} else if (!strcmp(vals[0], "ship.status")) {
		strncpy(ship->status, vals[1], 15);
	}
	return 0;
}


static void
load_ship_info(sqlite3 *db, Ship *ship)
{
	sqlite3_exec(db, "SELECT * FROM properties WHERE name LIKE 'ship.%'",
			&ship_info_callback, ship, NULL);
}

static int
finance_info_callback(void *vis, int cols, char **vals, char **names)
{
	IndexedState *is = vis;
	FinanceEntry *fe = is->data;

	(void)cols; /* supress warning */
	(void)names; /*supress warning */

	fe += *is->index;
	(*is->index)++;

	strncpy(fe->name, vals[0], FINANCE_ENTRY_LEN);
	fe->value = strtol(vals[1], NULL, 10);

	return 0;
}

static void
load_finance_info(sqlite3 *db, GameState *game)
{
	game->finance_count = 0;
	IndexedState is = { game->finances, &game->finance_count };
	char query[QUERY_LENGTH];

	sprintf(query, "SELECT * FROM finance ORDER BY rowid DESC LIMIT %d",
			FINANCE_ENTRY_COUNT);
	sqlite3_exec(db, query, &finance_info_callback, &is, NULL);
}

static void
load_gamestate(sqlite3 *db, GameState *game)
{
	load_ship_info(db, &game->ship);
	load_finance_info(db, game);
}

static void
publish_ship(Ship *ship, void *publisher)
{
	char message[SHIP_MESSAGE_LEN];
	int pos = 0;

	// TODO: use strncpy
	message[pos++] = SHIP_UPDATE;
	strcpy(message + pos, ship->name);
	pos += SHIPNAME_LEN;
	strcpy(message + pos, ship->status);
	pos += SHIPSTATUS_LEN;
	message[pos++] = ship->hull.current;
	message[pos++] = ship->hull.max;
	message[pos++] = ship->energy.current;
	message[pos++] = ship->energy.max;
	message[pos++] = ship->armor.current;
	message[pos++] = ship->armor.max;

	assert(zmq_send(publisher, message, pos, 0) == pos);
}

static void
publish_finances(GameState *gs, void *publisher)
{
	char message[FINANCE_MESSAGE_LEN];
	unsigned j;
	int i, pos = 0;
	long data;

	message[pos++] = FINANCE_UPDATE;
	message[pos++] = gs->finance_count;
	for (i = 0; i < gs->finance_count; i++) {
		strncpy(message + pos, gs->finances[i].name,
				FINANCE_ENTRY_LEN);
		pos += FINANCE_ENTRY_LEN;
		data = gs->finances[i].value;
		for (j = 0; j < sizeof(long); j++) {
			message[pos + j] = 0xFF & data;
			data >>= 8;
		}
		pos += j;
	}

	assert(zmq_send(publisher, message, pos, 0) == pos);
}

int
main(void)
{
	void *context, *server, *publisher;
	sqlite3 *db;
	GameState game;
	/* char buffer[BUFSIZE]; */
	/* int rc; */

	context = zmq_ctx_new();
	server = zmq_socket(context, ZMQ_REP);
	publisher = zmq_socket(context, ZMQ_PUB);

	assert(!zmq_bind(server, "tcp://*:5555"));
	assert(!zmq_bind(publisher, "tcp://*:5556"));
	assert(!sqlite3_open("pal.db", &db));

	load_gamestate(db, &game);

	while (1) {
		/*
		rc = zmq_recv(server, buffer, BUFSIZE - 1, 0);
		if (rc == BUFSIZE - 1)
			buffer[BUFSIZE - 1] = '\0';
		zmq_send(server, buffer, rc, 0);
		*/
		publish_ship(&game.ship, publisher);
		publish_finances(&game, publisher);
		load_gamestate(db, &game);
		sleep(1);
	}

	sqlite3_close(db);
	zmq_close(publisher);
	zmq_close(server);
	zmq_ctx_destroy(context);

	return 0;
}
