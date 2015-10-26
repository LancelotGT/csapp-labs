#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_type = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";

void handle_request(int clientfd);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int parse_url(char* url, char* hostname, char* uri);
void cat_requesthdrs(char* str, char* hostname);
void read_requesthdrs(rio_t *rp); 

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        if ((connfd = accept(listenfd, (SA *)&clientaddr, &clientlen)) == -1)
        {   /* block until get a request */
            printf("Accepting connection error\n");
            continue;
        }
        int rc;
        if ((rc = getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                port, MAXLINE, 0)) != 0)
        {
            printf("Getnameinfo error\n");
            continue;
        }
            
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        handle_request(connfd);
        if ((rc = close(connfd)) < 0)
        {
            printf("Close error\n");
            continue;
        }
        printf("Connection closed.\n");
    }
}

/*
 * handle_request - handle one HTTP request from client and 
 * make request for actual server. It will wait for response
 * and forward response back to client.
 */
void handle_request(int clientfd)
{
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE];
    char hostname[MAXLINE], uri[MAXLINE], version[MAXLINE];

    rio_t rio;
    char port[] = "80"; /* http request use port 80 */

    /* Read request line and headers */
    Rio_readinitb(&rio, clientfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);
    if (strcasecmp(method, "GET")) { 
        clienterror(clientfd, method, "501", "Not Implemented",
                "Proxy does not implement this method");
        return;
    }
    
    parse_url(url, hostname, uri);

    /* connect to host server by hostname */
    printf("hostname: %s\n", hostname);
    printf("url: %s\n", url); 
    printf("uri: %s\n", uri); 

    int serverfd;
    if ((serverfd = open_clientfd(hostname, port)) < 0)
    {
        printf("Open_clientfd error\n");
        return;
    }

    /* construct a new request line */
    char requestLine[MAXLINE];
    strcpy(requestLine, method);
    strcat(requestLine, uri);
    strcat(requestLine, "HTTP/1.0");
    rio_readinitb(&rio, serverfd);
    rio_writen(serverfd, buf, strlen(buf)); /* send request line to server */

    /* read and send request header to server */
    char requesthrds[MAXLINE];
    cat_requesthdrs(requesthrds, hostname);
    rio_writen(serverfd, requesthrds, strlen(requesthrds));

    //rio_readlineb(&rio, buf, MAXLINE);
    //rio_writen(serverfd, buf, strlen(buf));
    //while(strcmp(buf, "\r\n")) 
    //{
    //    rio_readlineb(&rio, buf, MAXLINE);
    //    rio_writen(serverfd, buf, strlen(buf));
    //} 
    char emptyline[] = "\r\n";
    rio_writen(serverfd, emptyline, strlen(emptyline));

    /* get response from server and send it back to client */
    rio_writen(clientfd, emptyline, strlen(emptyline));
    read_requesthdrs(&rio);
    size_t n;
    while ((n = rio_readlineb(&rio, buf, MAXLINE)) != 0) 
    {
        printf("Proxy received %lu bytes\n", n);
        rio_writen(clientfd, buf, strlen(buf));
    }
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * parse_url - parse the given url into a hostname and uri.
 */
int parse_url(char* url, char* hostname, char* uri)
{
    char *ptr;
    if (!(ptr = strstr(url, "http://")))
        return -1;
    printf("ptr: %s\n", ptr + 7);
    /* copy hostname from url */
    int i;
    for (i = 7; ptr[i] != '/' && ptr[i] != '\0'; i++)
        hostname[i - 7] = ptr[i];
    hostname[i - 7] = '\0';

    if (ptr[i] == '\0')
        strcpy(uri, "\\");
    else
	    strcpy(uri, ptr + i);
	return 0;
}

/*
 * cat_requesthdrs - concatenate the request headers given hostname.
 * This is used by proxy to send request to server
 */
void cat_requesthdrs(char* str, char* hostname)
{
    strcpy(str, "Host: ");
    strcat(str, hostname);
    strcat(str, "\r\n");
    strcat(str, user_agent);
    strcat(str, accept_type);
    strcat(str, accept_encoding);
    strcat(str, connection); 
    strcat(str, proxy_connection); 
}

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */ 
