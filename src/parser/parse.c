#include "parse.h"
#include "../utilities/commons.h"

extern void set_parsing_options(char *buf, size_t siz, Request *parsing_request);
extern int yyparse(void);

/**
 * Given a char buffer returns the parsed request headers.
 * Return fulfilled buffer on success.
 * Return NULL porinter on failure.
 */
Request * parse(char *buffer, int size, int socketFd) {
  //Differant states in the state machine
	enum {
		STATE_START = 0, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);

	state = STATE_START;
	while (state != STATE_CRLFCRLF) {
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state) {
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;

	}

  //Valid End State
	if (state == STATE_CRLFCRLF) {
		Request *request = (Request *) malloc(sizeof(Request));
    request->header_count=0;
    //TODO: Resizing this correctly
    request->headers = (Request_header *) malloc(sizeof(Request_header)*8);
		set_parsing_options(buf, i, request);

		if (yyparse() == SUCCESS) {
      return request;
		}
	}
  //TODO Handle Malformed Requests
  printf("Parsing Failed\n");
  return NULL;
}
