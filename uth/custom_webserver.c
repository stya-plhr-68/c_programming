#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>

#define PROT_HTTP11 "HTTP/1.1"

#define STAT_200 " 200 OK\r\n"
#define STAT_404 " 404 Not Found\r\n"
#define STAT_501 " 501 Not Implemented\r\n"

#define F_DIR "Content-Type: text/directory\r\n"
#define F_GIF "Content-Type: image/gif\r\n"
#define F_HTML "Content-Type: text/html\r\n"
#define F_ICO "Content-Type: image/x-icon\r\n"
#define F_JPEG "Content-Type: image/jpeg\r\n"
#define F_JPG "Content-Type: image/jpg\r\n"
#define F_TXT "Content-Type: text/plain\r\n"

typedef enum {cgi, gif, html, ico, jpeg, jpg, plain, noext} ext;

ext get_ext(char *file) {
    if (strstr(file, ".cgi") != NULL)
        return cgi;
    if (strstr(file, ".gif") != NULL)
        return gif;
    if (strstr(file, ".html") != NULL)
        return html;
    if (strstr(file, ".ico") != NULL)
    return ico;
    if (strstr(file, ".jpeg") != NULL)
        return jpeg;
    if (strstr(file, ".jpg") != NULL)
        return jpg;
    if (strstr(file, ".txt") != NULL)
        return plain;
    return noext;
}

char webpage[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!doctype html>\r\n"
"<html><head><title>B2badmin</title>\r\n"
"<style>body {background-color: #FFF00 }</style></head>\r\n"
"<body><center><h1>Hello world!</h1><br>\r\n"
"<img src=\"test.jpg\"></center></body></html>\r\n";

char jpgheader[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: image/jpeg\r\n\r\n";

char icoheader[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: image/x-icon\r\n\r\n";

char * get_url(char *);
char * get_fname(char *);
long long get_f_size(char *);
void DieWithError(char *errorMessage);
int CreateTCPServerSocket (unsigned short port); 
void SendToClientSocket(int clntSock, char *buf);

int main(int argc, char *argv[])
{
    struct sockaddr_in client_addr;
    socklen_t sin_len = sizeof(client_addr);
    int servSock , clntSock;
    char buf[2048];
    pid_t processID;
    int status;
    unsigned int childProcCount = 0;

    servSock = CreateTCPServerSocket(58080);

    while(1)
    {
        clntSock = accept(servSock, (struct sockaddr *) &client_addr, &sin_len);
        if(clntSock == -1)
        {
            perror("Connection failed...\n");
            continue;
        }

        printf("Got client connection...\n");
        if ((processID = fork()) < 0)
        {
            DieWithError("fork() failed");
        }
        else if ( processID == 0 ) {
            /* in child */
            close(servSock);
            memset(buf, 0, 2048);
            read(clntSock, buf, 2047);
            SendToClientSocket(clntSock, buf);
            printf("closing...\n");
            close(clntSock);
            exit(0);
        }
        else {
            /* in parent */
            
            // printf ("In parent \n");
            // if (waitpid(child, NULL, 0) < 0) {
            //     perror("Failed to collect child process");
            //     break;
            // }


            printf("with child process' %d\n", (int) processID);
            close(clntSock);
            /* Parent closes child socket descriptor */
            childProcCount++;
            /* Increment number of outstanding child processes */
            while (childProcCount) /* Clean up all zombies */
            {
                processID = waitpid((pid_t) -1, NULL, WNOHANG); /* Nonblocking wait */
                if (processID < 0) /* waitpid() error? */
                    DieWithError("waitpid() failed");
                else if (processID == 0) /* No zombie to wait on */
                    break;
                else
                    childProcCount--; /* Cleaned up after a child */
            }
            
        }
        /* parent process */
        close(clntSock);

    }
    return 0;
}

char * get_url(char * buf)
{
    char *p1 = strstr(buf, " ")+1;
    char *p2 = strstr(p1, " ");
    int len = p2-p1;
    char *url;
    url = (char *) malloc(2048);
    memset(url, '\0', 2048);
    //printf("%d\n",len);
    strncpy(url, p1, len);
    //printf("%s\n",url);
    return url;
}

char * get_fname(char * buf)
{
    return basename(strdup(get_url(buf)));
}

long long get_f_size(char * buf)
{
    long long size;
    struct stat st;
    stat(get_fname(buf), &st);
    size = st.st_size;
    printf("The size of file is : %ld\n",size);
    return size;
}

void DieWithError(char *errorMessage)
{
    perror ( errorMessage) ;
    exit(1);
}

int CreateTCPServerSocket (unsigned short port)
{
    int servSock;
    int on =1;
    struct sockaddr_in server_addr;
    servSock = socket(AF_INET, SOCK_STREAM, 0);
    if(servSock < 0)
    {
        perror("socket");
        exit(1);
    }

    setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if(bind(servSock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(servSock);
        exit(1);
    }

    if(listen(servSock, 10) ==-1)
    {
        perror("listen");
        close(servSock);
        exit(1);
    }
    return servSock;
}

void SendToClientSocket(int clntSock, char *buf)
{
    char *url, *f_name;
    ext f_ext;
    long long f_size;
    int fdimg;
    int retsendfile;

    url = get_url(buf);
    f_name = get_fname(buf);
    f_ext = get_ext(f_name);
    printf("%s\n", buf);
    if(f_ext != noext)
    { 
        f_size = get_f_size(buf);
        fdimg = open(f_name, O_RDONLY); 
        if(fdimg >0 )
        {
            if (f_ext == jpg) {
                write(clntSock, jpgheader, sizeof(jpgheader) - 1);
            }
            else if (f_ext == ico) {
                write(clntSock, icoheader, sizeof(icoheader) - 1);
            }
            retsendfile = sendfile(clntSock, fdimg, NULL, f_size);
            if(retsendfile==-1)
            {
                perror("send file \n");
            }
            else
            {
                printf("%f bytes send to client for file %s \n", retsendfile,f_name);
            }
            close(fdimg);
        }
        else
        {
            perror("File not found");
            write(clntSock, webpage, sizeof(webpage) - 1);
        }
        
    }
    else
    {
        write(clntSock, webpage, sizeof(webpage) - 1);
    }
    return;
}