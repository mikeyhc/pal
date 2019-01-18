#include <assert.h>
#include <stdio.h>
#include <zmq.h>

#include "pal.h"

int
main(void)
{
	void *context;
	void *subscriber;
	char buffer[MAX_MESSAGE_LEN];

	context = zmq_ctx_new();
	subscriber = zmq_socket(context, ZMQ_SUB);
	assert(!zmq_connect(subscriber, "tcp://localhost:5556"));
	assert(!zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0));

	printf("max message length: %d\n", MAX_MESSAGE_LEN);

	while (1) {
		zmq_recv(subscriber, buffer, MAX_MESSAGE_LEN, 0);
		printf("got messaŋe\n");
	}

	zmq_close(subscriber);
	zmq_ctx_destroy(context);
}

