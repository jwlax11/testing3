#include <stdio.h>
#include <string.h>
#include "proxy.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char** argv)
{
    char port[MAXLINE];
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    memset(port, 0, MAXLINE * sizeof(char));
    strcpy(port, argv[1]);\
    printf("Port: %s\n", port);
    printf("%s", user_agent_hdr);

    RunServer(port);
    return 0;
}

void *thread(void *argp)
{
    int *pt = argp;
    int clientfd = *pt;
    ProcessClientRequest(clientfd);
    Close(clientfd);
    printf("Closed connection to client\n");
    return NULL;
}


int RunServer(char* port)
{
    int listenfd, clientfd;
    socklen_t clientlen;
    char hostname[MAXLINE];
    struct sockaddr_storage clientaddr;
    listenfd = Open_listenfd(port);
    while(1)
    {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA * ) & clientaddr, &clientlen);
        Getnameinfo((SA *) & clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        pthread_t tid;
        Pthread_create(&tid, NULL, thread, &clientfd);
    }
    return listenfd;
}

void ProcessClientRequest(int clientfd)
{
	char	buf [MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char    filename  [MAXLINE];
	rio_t	rio;

    // Read request line and headers
    Rio_readinitb(&rio, clientfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
    {
        return;
    }
    printf("Buffer: %s\n", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("Method: %s\n", method);
    printf("URI: %s\n", uri);
    printf("Version: %s\n", version);
    if (strcasecmp(method, "GET"))
    {
        clienterror(clientfd, method, "501", "Not Implemented Yet", "Working only on GET");
        return;
    }
    else
    {
        int serverfd;
        version[strlen(version) - 1] = '1';
        printf("Version: %s\n", version);
        char request[MAXLINE], host[MAXLINE];
        CreateRequest(rio, request, method, uri, host, filename,  version);
        serverfd = SendRequestToServer(request, host, filename);
        ForwardResponse(serverfd, clientfd);
        Close(serverfd);
        printf("Closed connection to server\n");
    }
}

void CreateRequest(rio_t rio, char* request, char* method, char* uri, char* hostname, char* filename, char* version)
{
    char file[MAXLINE], buf[MAXLINE], temp[MAXLINE];
    sscanf(uri, "http://%s", buf);
    sscanf(buf, "%[^'/']/%s", hostname, temp);
    sprintf(file, "/%s", temp);
    printf("\n\nHostname: %s\n", hostname);
    printf("file: %s\n\n", file);

    sprintf(request, "%s %s %s\r\n", method, file, version);
    sprintf(request, "%sHost: %s\r\n", request, hostname);
    // sprintf(request, "%sUser-Agent: %s\r\n", request, user_agent_hdr);
    sprintf(request, "%sProxy-Connection: %s\r\n", request, "Close");
    sprintf(request, "%sConnection: %s\r\n", request, "Close");

    read_requesthdrs(&rio, request);
    sprintf(request, "%s\r\n", request);
    printf("\nRequest:\n%s", request);
}

int SendRequestToServer(char* request, char* host, char* file)
{
    char port[MAXLINE], hostname[MAXLINE];
    int serverfd;

    sscanf(host, "%[^':']:%[^':']", hostname, port);
    printf("Host: %s\nPort: %s\n", hostname, port);

    serverfd = Open_clientfd(hostname, port);
    Rio_writen(serverfd, request, strlen(request));
    return serverfd;
}

void ForwardResponse(int serverfd, int clientfd)
{
    printf("\nEntering ForwardResponse\n");
    // char buf[MAX_OBJECT_SIZE * sizeof(char)], response[MAX_OBJECT_SIZE * sizeof(char)];
    char* buf = (char*) malloc(MAX_OBJECT_SIZE * sizeof(char));
    char* response = (char*) malloc(MAX_OBJECT_SIZE * sizeof(char));
    memset(buf, 0, strlen(buf));
    memset(response, 0, strlen(buf));
    rio_t rio;
    int respLen;
    printf("Reading the response from the server\n");
    Rio_readinitb(&rio, serverfd);
    do
    {  
        char key[MAXLINE], value[MAXLINE];
        Rio_readlineb(&rio, buf, MAXLINE);
        sscanf(buf, "%s %s", key, value);
        if (strcmp(key, "Content-length:") == 0)
        {
            respLen = atoi(value);
            printf("Parsed content length: %d", respLen);
        }
        sprintf(response,"%s%s", response, buf);
    } while(strcmp(buf, "\r\n"));
    printf("Sending headers\n");
    printf("%s", response);
    Rio_writen(clientfd, response, strlen(response));
    
    printf("Reading body of %d bytes\n", respLen);
    Rio_readnb(&rio, buf, respLen);
    printf("%s\n", buf);
    // sprintf(response, "%s%s", response, buf);
    printf("Response:\n%s", response);
    printf("Writing %d bytes\n", respLen);
    printf("%s\n", buf);
    Rio_writen(clientfd, buf, respLen);
    // free(buf);
    // free(response);
}

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t * rp, char* request)
{
    char buf[MAXLINE];
	Rio_readlineb(rp, buf, MAXLINE);
    int i = 0;
	// printf("%d: %s", i++, buf);
	while (strcmp(buf, "\r\n"))
	{
        char key[MAXLINE], value[MAXLINE];
		//line: netp: readhdrs:checkterm
		Rio_readlineb(rp, buf, MAXLINE);
		// printf("%d: %s", i, buf);
        sscanf(buf, "%s %s", key, value);
        if (strcmp(key, "User-Agent:") != 0 && strcmp(key, "Proxy-Connection:") != 0
            && strcmp(key, "Host:") != 0)
        {
            sprintf(request, "%s%s", request, buf);
        }
        i++;
	}
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Proxy Error</title>");
	sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}