#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

#define MAXWORD 128

typedef struct {
    char host[MAXWORD];
    char port[MAXWORD];
    char path[MAXWORD];
} url_t;


int parse_url(char *url_str, url_t *url_info) {
    // HTTP protocol
    if (strncasecmp(url_str, "http://", strlen("http://")) != 0) {
        fprintf(stderr, "not http protocol: %s\n", url_str);
        return -1;
    }

    char *host_ptr = url_str + strlen("http://");
    char *port_ptr = strchr(host_ptr, ':');
    char *uri_ptr = strchr(host_ptr, '/');

    if (!uri_ptr)
        return -1;

    if (!port_ptr) {
        *uri_ptr = '\0';
        strcpy(url_info->host, host_ptr);
        strcpy(url_info->port, "80");
        *uri_ptr = '/';
        strcpy(url_info->path, uri_ptr);
    } else {
        *port_ptr = '\0';
        strcpy(url_info->host, host_ptr);
        *uri_ptr = '\0';
        strcpy(url_info->port, port_ptr + 1);
        *uri_ptr = '/';
        strcpy(url_info->path, uri_ptr);
    }
    return 0;
}

void parse_header(rio_t *rio_ptr, char *header, url_t *url_info) {
    char buffer[MAXLINE];

    // method URI version
    sprintf(buffer, "GET %s HTTP/1.0\r\n", url_info->path);
    strcat(header, buffer);

    // host header
    sprintf(buffer, "Host: %s\r\n", url_info->host);
    strcat(header, buffer);

    // user agent
    sprintf(buffer, "User-Agent: %s", user_agent_hdr);
    strcat(header, buffer);

    // connection and proxy-connection headers
    strcat(header, "Connection: close\r\n");
    strcat(header, "Proxy-Connection: close\r\n");

    while (strcmp(buffer, "\r\n") != 0) {
        rio_readlineb(rio_ptr, buffer, MAXLINE);
        if (strncasecmp(buffer, "User-Agent:", strlen("User-Agent:")) == 0) {
            continue;
        }
        if (strncasecmp(buffer, "Connection:", strlen("Connection:")) == 0) {
            continue;
        }
        if (strncasecmp(buffer, "Proxy-Connection:", strlen("Proxy-Connection:")) == 0) {
            continue;
        }
        strcat(header, buffer);
    }
    strcat(header, "\r\n");
}

void handle_request(rio_t *rio_ptr, char *url_str) {
    url_t url_info;
    if (parse_url(url_str, &url_info) == -1) {
        fprintf(stderr, "parse URL error\n");
        return;
    }

    char header[MAXLINE];
    parse_header(rio_ptr, header, &url_info);

    int serverfd = open_clientfd(url_info.host, url_info.port);
    if (serverfd < 0) {
        fprintf(stderr, "connect server error: %s:%s\n", url_info.host, url_info.port);
        return;
    }

    rio_t server_rio;
    rio_readinitb(&server_rio, serverfd);
    if (rio_writen(serverfd, header, strlen(header)) != strlen(header)) {
        fprintf(stderr, "send request header error\n");
        close(serverfd);
        return;
    }

    int total = 0, curr = 0;
    char buf[MAXLINE];
    while ((curr = rio_readnb(&server_rio, buf, MAXLINE)) != 0) {
        if (rio_writen(rio_ptr->rio_fd, buf, curr) != curr) {
            fprintf(stderr, "send response error\n");
            close(serverfd);
            return;
        }
        total += curr;
    }
    close(serverfd);
}

void *thread(void *vargp) {
    pthread_detach(pthread_self());

    int connfd = *((int *) vargp);
    free(vargp);

    rio_t client_rio;
    char buffer[MAXLINE];
    rio_readinitb(&client_rio, connfd);

    if (rio_readlineb(&client_rio, buffer, MAXLINE) < 0) {
        fprintf(stderr, "read request error: %s\n", strerror(errno));
        close(connfd);
        return NULL;
    }

    char method[MAXWORD], url[MAXWORD], version[MAXWORD];
    if (sscanf(buffer, "%s %s %s", method, url, version) != 3) {
        fprintf(stderr, "read request error: %s\n", strerror(errno));
        close(connfd);
        return NULL;
    }

    if (strcasecmp(method, "GET") == 0) {
        handle_request(&client_rio, url);
    }
    close(connfd);
    return NULL;
}

int main(int argc, char* argv[]) {
    int listenfd, *connfdp;
    socklen_t client_len;
    struct sockaddr_storage client_addr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        client_len = sizeof(struct sockaddr_storage);
        connfdp = (int *) malloc(sizeof(int));

        // wait for a connection request
        if ((*connfdp = accept(listenfd, (SA *) &client_addr, &client_len)) < 0) {
            fprintf(stderr, "accept Error: %s\n", strerror(errno));
            continue;
        }

        // create a new thread to handle the request
        pthread_create(&tid, NULL, thread, connfdp);
    }
    close(listenfd);
}
