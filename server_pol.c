/*******************************************************************************
* Multi-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches multiple threads to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*     workers     - The number of workers to start to process requests
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 29, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> "		\
	"-w <workers> "				\
	"-p <policy: FIFO | SJN> "		\
	"<port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* Mutex needed to protect the threaded printf. DO NOT TOUCH */
sem_t * printf_mutex;

/* Synchronized printf for multi-threaded operation */
#define sync_printf(...)			\
	do {					\
		sem_wait(printf_mutex);		\
		printf(__VA_ARGS__);		\
		sem_post(printf_mutex);		\
	} while (0)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

enum queue_policy {
	QUEUE_FIFO,
	QUEUE_SJN
};

int poli = 0;
int worker_done = 0;
double tot_response_time = 0.0;
double response_times[1500];

struct request_meta {
	struct request request;
	struct timespec receipt_timestamp;
	struct timespec start_timestamp;
	struct timespec completion_timestamp;
};

struct queue {
	size_t wr_pos;
	size_t rd_pos;
	size_t max_size;
	size_t available;
	struct request_meta * requests;
};

struct connection_params {
	size_t queue_size;
	int num_workers;
	enum queue_policy policy;
};

struct worker_params {
	int thread_id;
	int conn_socket;
	//int worker_done;
	struct queue * the_queue;
};

/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
	/* IMPLEMENT ME !! */
	the_queue->rd_pos = 0;
	the_queue->wr_pos = 0;
	the_queue->max_size = queue_size;
	the_queue->requests = (struct request_meta *)malloc(sizeof(struct request_meta)
						     * the_queue->max_size);
	the_queue->available = queue_size;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Make sure that the queue is not full */
	if (the_queue->available == 0) {
		retval = 1;
		/* What to do in case of a full queue */
		printf("I AM FULL \n");
		/* DO NOT RETURN DIRECTLY HERE. The
		 * sem_post(queue_mutex) below MUST happen. */
	} else {
		/* If all good, add the item in the queue */
		/* IMPLEMENT ME !!*/
		the_queue->requests[the_queue->wr_pos] = to_add;
		the_queue->wr_pos = (the_queue->wr_pos + 1) % the_queue->max_size;
		the_queue->available--;

		/* OPTION 1: After a correct ADD operation, sort the
		 * entire queue. */

		/* OPTION 2: Find where to place the request in the
		 * queue and shift all the other entries by one
		 * position to the right. */

		/* OPTION 3: Do nothing different from FIFO case,
		 * and deal with the SJN policy at dequeue time.*/

		/* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
		sem_post(queue_notify);
	}

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Option 3-A: Scan the queue to find the shortest request,
	 * pop it, and shift all the other requests by one spot to the
	 * left. */
	if (poli == 1){
		//printf("I SHOULD BE SJN GET QUEUE: %d \n", poli);
		struct request_meta shortest_req = the_queue->requests[the_queue->rd_pos];
		int shortest_idx = the_queue->rd_pos;

		for (int i = (the_queue->rd_pos + 1) % the_queue->max_size; i != the_queue->wr_pos; i = (i + 1) % the_queue->max_size) {
			if (TSPEC_TO_DOUBLE(the_queue->requests[i].request.req_length) < TSPEC_TO_DOUBLE(shortest_req.request.req_length)) {
				shortest_req = the_queue->requests[i];
				shortest_idx = i;
			}
		}

		/* Remove the shortest request from the queue and shift the others. */
		for (int i = shortest_idx; i != the_queue->wr_pos; i = (i + 1) % the_queue->max_size) {
			int next = (i + 1) % the_queue->max_size;
			the_queue->requests[i] = the_queue->requests[next];
		}

		the_queue->wr_pos = (the_queue->wr_pos - 1 + the_queue->max_size) % the_queue->max_size;
		the_queue->available++;
	}else {
		//printf("I SHOULD BE FIFO GET QUEUE: %d \n", poli);
		retval = the_queue->requests[the_queue->rd_pos];
		the_queue->rd_pos = (the_queue->rd_pos + 1) % the_queue->max_size;
		the_queue->available++;
	}

	/* Option 3-B: Sort the entire queue and pop the request in
	 * the first position. */

	/* NOTE: all these options only apply if you have implemented
	 * your queue as an array. If you have employed a linked list,
	 * the sorted insert approach is definitely the winner. Also,
	 * in this case you are a wizard, Harry. */

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

void dump_queue_status(struct queue * the_queue)
{
	size_t i, j;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	printf("Q:[");

	for (i = the_queue->rd_pos, j = 0; j < the_queue->max_size - the_queue->available;
	     i = (i + 1) % the_queue->max_size, ++j)
	{
		printf("R%ld%s", the_queue->requests[i].request.req_id,
		       ((j+1 != the_queue->max_size - the_queue->available)?",":""));
	}

	printf("]\n");
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

/* Main logic of the worker thread */
int worker_main (void * arg)
{
	struct timespec now;
	struct worker_params * params = (struct worker_params *)arg;
	int thread_id = params->thread_id;
	/* Print the first alive message. */
	clock_gettime(CLOCK_MONOTONIC, &now);
    sync_printf("[#WORKER#] %lf Worker Thread %d Alive!\n", TSPEC_TO_DOUBLE(now), thread_id);

	/* Okay, now execute the main logic. */
    while (1) {
        if (worker_done != 0) {
            sync_printf("Worker Thread %d Exiting\n", thread_id);
            break; // exit the loop if worker_done is set and the queue is empty.
        }
        else {
            struct request_meta req;
            struct response resp;

			req = get_from_queue(params->the_queue);
			clock_gettime(CLOCK_MONOTONIC, &req.start_timestamp);
			busywait_timespec(req.request.req_length);
			clock_gettime(CLOCK_MONOTONIC, &req.completion_timestamp);

			tot_response_time += TSPEC_TO_DOUBLE(req.completion_timestamp) - TSPEC_TO_DOUBLE(req.receipt_timestamp);
			response_times[req.request.req_id] = TSPEC_TO_DOUBLE(req.completion_timestamp) - TSPEC_TO_DOUBLE(req.receipt_timestamp);

			/* Now provide a response! */
			resp.req_id = req.request.req_id;
			resp.ack = 0;
			send(params->conn_socket, &resp, sizeof(struct response), 0);

			/* Print the completion information with thread ID */
			sync_printf("T%d R%ld:%lf,%lf,%lf,%lf,%lf\n", 
				thread_id, req.request.req_id,
				TSPEC_TO_DOUBLE(req.request.req_timestamp),
				TSPEC_TO_DOUBLE(req.request.req_length),
				TSPEC_TO_DOUBLE(req.receipt_timestamp),
				TSPEC_TO_DOUBLE(req.start_timestamp),
				TSPEC_TO_DOUBLE(req.completion_timestamp)
			);
			dump_queue_status(params->the_queue);
		}
	}
	return EXIT_SUCCESS;
}

/* This function will start the worker thread wrapping around the
 * clone() system call*/
int start_worker(void * params, void * worker_stack)
{
	/* IMPLEMENT ME !! */
	int retval;

	/* Throw an error if no stack was passed. */
	if (worker_stack == NULL)
		return -1;

	retval = clone(worker_main, worker_stack + STACK_SIZE,
		       CLONE_THREAD | CLONE_VM | CLONE_SIGHAND |
		       CLONE_FS | CLONE_FILES | CLONE_SYSVSEM,
		       params);

	return retval;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct request_meta * req;
	struct queue * the_queue;
	size_t in_bytes;
	double tot_request_len;
	struct timespec now, end;
	int num_request = 0;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if(conn_params.policy == QUEUE_SJN){
		poli = 1;
	}

	/* IMPLEMENT ME!! Write a loop to start and initialize all the
	 * worker threads ! */
	pid_t worker_pids[conn_params.num_workers];
    struct worker_params worker_data[conn_params.num_workers];

    // Initialize the queue
	the_queue = (struct queue *)malloc(sizeof(struct queue));
    queue_init(the_queue, conn_params.queue_size);

    for (int i = 0; i < conn_params.num_workers; i++) {
        worker_data[i].thread_id = i;
        worker_data[i].conn_socket = conn_socket;
        //worker_data[i].worker_done = 0;
        worker_data[i].the_queue = the_queue;
		char* stack = malloc(STACK_SIZE);

        worker_pids[i] = start_worker(&worker_data[i], stack);
		printf("INFO: Worker thread started. Thread ID = %d\n", worker_data[i].thread_id);
    }

	/* We are ready to proceed with the rest of the request
	 * handling logic. */

	req = (struct request_meta *)malloc(sizeof(struct request_meta));

	do {
		/* IMPLEMENT ME: Receive next request from socket. */

		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */
		in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &req->receipt_timestamp);

		tot_request_len += TSPEC_TO_DOUBLE(req->request.req_length);

		/* IMPLEMENT ME: Attempt to enqueue or reject request! */
		if (in_bytes > 0) {
			/* IMPLEMENT ME: Attempt to enqueue or reject request! */
			//add_to_queue(*req, the_queue);
			if (add_to_queue(*req, the_queue)){ //got rejected
				//rejectnum ++;
				struct response resp;
				resp.req_id = req->request.req_id;
				resp.ack = 1;
				send(conn_socket, &resp, sizeof(struct response), 0);
				sync_printf("X%ld:%lf,%lf,%lf\n", req->request.req_id, TSPEC_TO_DOUBLE(req->request.req_timestamp),TSPEC_TO_DOUBLE(req->request.req_length),TSPEC_TO_DOUBLE(req->receipt_timestamp));
			}else{
				num_request ++;
			}
			//requestnum ++;
		}
	} while (in_bytes > 0);

	/* IMPLEMENT ME!! Write a loop to gracefully terminate all the
	 * worker threads ! */
	worker_done = 1;
	//sem_post(queue_notify);
	
	for (int i = 0; i < conn_params.num_workers; i++) {
		int status;
		waitpid(worker_pids[i], &status, 0);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

/* 	for (int i = 0; i < num_request; i++)
	{
		if(i == (num_request-1)){
			printf("%f] \n", response_times[i]);
			break;
		}
		if(i == 0){
			printf("Response Times = [");
		}
		printf("%f, ", response_times[i]);
	} */
	

	double avg_response_time = tot_response_time/num_request;
	double tot_server_time = TSPEC_TO_DOUBLE(end) - TSPEC_TO_DOUBLE(now);
	double ulility = tot_request_len/tot_server_time;

	//printf("this is num requests: %d \n", num_request);

	//printf("this is utili: %f \n", ulility);
	//printf("this is Tq: %f \n", avg_response_time);

	free(the_queue);

	free(req);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;

	struct connection_params conn_params;

	/* Parse all the command line arguments */
    while ((opt = getopt(argc, argv, "q:w:p:")) != -1) {
        switch (opt) {
            case 'q':
                conn_params.queue_size = strtol(optarg, NULL, 10);
                //printf("INFO: setting queue size as %ld\n", conn_params.queue_size);
                break;
            case 'w':
                conn_params.num_workers = strtol(optarg, NULL, 10);
                //printf("INFO: setting number of workers as %d\n", conn_params.num_workers);
                break;
            case 'p':
                if (strcmp(optarg, "FIFO") == 0) {
                    conn_params.policy = QUEUE_FIFO;
                    printf("INFO: setting policy as FIFO\n");
				}else if(strcmp(optarg, "SJN") == 0) {
					conn_params.policy = QUEUE_SJN;
					printf("INFO: setting policy as SJN\n");
				}else{
                    fprintf(stderr, "ERROR: Invalid policy. Use 'FIFO' or 'SJN'\n");
                    fprintf(stderr, USAGE_STRING, argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            default: /* '?' */
                fprintf(stderr, USAGE_STRING, argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (!conn_params.queue_size || !conn_params.num_workers) {
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    if (optind < argc) {
        socket_port = strtol(argv[optind], NULL, 10);
        printf("INFO: setting server port as %d\n", socket_port);
    } else {
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* Convert INADDR_ANY into network byte order */
	any_address.s_addr = htonl(INADDR_ANY);

	/* Time to bind the socket to the right port  */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	/* Attempt to bind the socket with the given parameters */
	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to bind socket");
		return EXIT_FAILURE;
	}

	/* Let us now proceed to set the server to listen on the selected port */
	retval = listen(sockfd, BACKLOG_COUNT);

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to listen on socket");
		return EXIT_FAILURE;
	}

	/* Ready to accept connections! */
	printf("INFO: Waiting for incoming connection...\n");
	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

	if (accepted == -1) {
		ERROR_INFO();
		perror("Unable to accept connections");
		return EXIT_FAILURE;
	}

	/* Initilize threaded printf mutex */
	printf_mutex = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(printf_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize printf mutex");
		return EXIT_FAILURE;
	}

	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	retval = sem_init(queue_notify, 0, 0);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables */

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, conn_params);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}
