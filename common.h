/*

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <math.h>

/* Includes that are specific for TCP/IP */
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

/* Includes that are specific for TCP/IP */
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

/* Include our own timelib */
#include "timelib.h"

/* Define the value of 0 for a positive acknowledgement and 1 for
 * negative acknowledgement (rejected request) */
#define RESP_COMPLETED  0
#define RESP_REJECTED   1

/* This is a handy definition to print out runtime errors that report
 * the file and line number where the error was encountered. */
#define ERROR_INFO()							\
	do {								\
		fprintf(stdout, "Runtime error at %s:%d\n", __FILE__, __LINE__); \
		fflush(stdout);						\
	} while(0)

/* A simple macro to convert between a struct timespec and a double
 * representation of a timestamp. */
#define TSPEC_TO_DOUBLE(spec)				\
    ((double)(spec.tv_sec) + (double)(spec.tv_nsec)/NANO_IN_SEC)

/* Request payload as sent by the client and received by the
 * server. */
struct request {
	uint64_t req_id;
	struct timespec req_timestamp;
	struct timespec req_length;
};

/* Response payload as sent by the server and received by the
 * client. */
struct response {
	uint64_t req_id;
	uint8_t  ack;
};

