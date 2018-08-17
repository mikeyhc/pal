#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *context = zmq_ctx_new();
	void *publisher = zmq_socket(context, ZMQ_PUB);

	assert(!zmq_bind(publisher, "tcp://*:5556"));
	while (1) {
		zmq_send(publisher, " ", 1, 0);
		usleep(500);
	}
	zmq_close(publisher);
	zmq_ctx_destroy(context);
	return 0;
}
