#include "csapp.h"

int RunServer(char* port);
void ProcessClientRequest(int clientfd);
void CreateRequest(rio_t rio, char* headers, char* method, char* hostname, char* uri, char* filename, char* version);
int SendRequestToServer(char* request, char* host, char* file);
void ForwardResponse(int serverfd, int clientfd);

void read_requesthdrs(rio_t * rp, char* request);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
