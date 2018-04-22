// Multi-Threaded web server using posix pthreads
// BK Turley 2011
// http://kturley.com/simple-multi-threaded-web-server-written-in-c-using-pthreads/
 
//this simple web server is capable of serving simple html, jpg, gif & text files
 
//----- Include files ---------------------------------------------------------
#include <stdio.h>          // for printf()
#include <stdlib.h>         // for exit()
#include <string.h>         // for strcpy(),strerror() and strlen()
#include <fcntl.h>          // for file i/o constants
#include <sys/stat.h>       // for file i/o constants
#include <errno.h>
 
/* FOR BSD UNIX/LINUX  ---------------------------------------------------- */
#include <unistd.h>
#include <sys/types.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     /* for socket system calls   */
#include <arpa/inet.h>      /* for socket system calls (bind)  */
#include <sched.h>   
#include "qthread.h"        /* P-thread implementation        */    
#include <signal.h>         /* for signal                     */ 
// #include <semaphore.h>      /* for p-thread semaphores        */
/* ------------------------------------------------------------------------ */ 
 
//----- HTTP response messages ----------------------------------------------
#define OK_IMAGE    "HTTP/1.0 200 OK\nContent-Type:image/gif\n\n"
#define OK_TEXT     "HTTP/1.0 200 OK\nContent-Type:text/html\n\n"
#define NOTOK_404   "HTTP/1.0 404 Not Found\nContent-Type:text/html\n\n"
#define MESS_404    "<html><body><h1>FILE NOT FOUND</h1></body></html>"
#define MOVED_302   "HTTP/1.0 302 Found\nLocation: /index.html\n\n"

//----- Defines -------------------------------------------------------------
#define BUF_SIZE            1024 /* buffer size in bytes */
#define PEND_CONNECTIONS     100 /* pending connections to hold  */
#define TRUE                   1
#define FALSE                  0
 
/* Child thread implementation ----------------------------------------- */
void *my_thread(void * arg)
{
    int    myClient_s;          /* copy socket */
     
    /* other local variables ------------------------------------------------ */
    char           in_buf[BUF_SIZE];           // Input buffer for GET resquest
    char           out_buf[BUF_SIZE];          // Output buffer for HTML response
    char           *file_name;                 // File name
    unsigned int   buf_len;                    // Buffer length for file reads
    int   retcode;                    // Return code
    char           *p;
    
    myClient_s = (unsigned int)arg;        // copy the socket
 
    /* receive the first HTTP request (HTTP GET) ------- */
    do {
        retcode = qthread_recv(myClient_s, in_buf, BUF_SIZE, 0);
        if (retcode == 0){
            return 0;
        }
        if (retcode < 0){
            continue;
        }
    } while (retcode == 0);
 
    /* if receive error --- */
    if (retcode < 0){
        printf("Client %d exited\n", myClient_s);
        qthread_exit(NULL);
        return 0;
    }
    
    /* if HTTP command successfully received --- */
    else {
        unsigned int fh;  // File handle (file descriptor)
        /* Parse out the filename from the GET request --- */
        strtok_r(in_buf, " ", &p);
        file_name = strtok_r(NULL, " ", &p);
 
        /* Open the requested file (start at 2nd char to get rid */
        /* of leading "\") */
        fh = open(&file_name[1], O_RDONLY, S_IREAD | S_IWRITE);
   
        if (!strcmp(file_name, "/")) {
            strcpy(out_buf, MOVED_302);
            qthread_send(myClient_s, out_buf, strlen(out_buf), 0);
        }
        else if (!strcmp(file_name, "/index.html")) {
            strcpy(out_buf, OK_TEXT);
            qthread_send(myClient_s, out_buf, strlen(out_buf), 0);
            /* god this is a hack... */
            char *cmd = "echo '<UL>'; for f in *; do if [ -d $f ] ; then echo '<LI>'$f'</LI>';"
                "else echo '<LI><a href=\"/'$f'\">'$f'</a></LI>'; fi; done; echo '</UL>'";
            FILE *fp = popen(cmd, "r");
            while (1) {
                int n = fread(out_buf, 1, BUF_SIZE, fp);
                qthread_send(myClient_s, out_buf, n, 0);
                if (n < BUF_SIZE)
                    break;
            }
        }
        
        /* Generate and send the response (404 if could not open the file) */
        else if (fh == -1) {
            printf("File %s not found - sending HTTP 404 \n", &file_name[1]);
            strcpy(out_buf, NOTOK_404);
            qthread_send(myClient_s, out_buf, strlen(out_buf), 0);
            strcpy(out_buf, MESS_404);
            qthread_send(myClient_s, out_buf, strlen(out_buf), 0);
        }
        else {
            if ((strstr(file_name, ".jpg") != NULL) ||
                (strstr(file_name, ".gif") != NULL)) {
                strcpy(out_buf, OK_IMAGE);
            }
            else {
                strcpy(out_buf, OK_TEXT);
            }
            qthread_send(myClient_s, out_buf, strlen(out_buf), 0);
 
            buf_len = 1;  
            while (buf_len > 0) {
                buf_len = qthread_read(fh, out_buf, BUF_SIZE);
                /* hack - we'll assume send is non-blocking */
                if (buf_len > 0){
                    qthread_send(myClient_s, out_buf, buf_len, 0);     
                }
            }
        }
        close(fh);       // close the file
        close(myClient_s); // close the client connection
        printf("Client %d exited\n", myClient_s);
        qthread_exit(NULL);
    }
    return 0;
}

//===== Main program ========================================================
void *main_thread(void *arg)
{
    int server_s = *(int*)arg;
    struct sockaddr_in    client_addr;            // Client Internet address
    struct in_addr        client_ip_addr;         // Client IP address
    unsigned int          addr_len;               // Internet address length
    
    /* the web server main loop ============================================= */
    while(TRUE) {
        printf("server ready ...\n");  
 
        /* wait for the next client to arrive -------------- */
        addr_len = sizeof(client_addr);
        int client_s = qthread_accept(server_s, (struct sockaddr *)&client_addr,
                                      &addr_len);
 
        if (client_s == FALSE) {
            perror("socket");
            exit(1);
        }
        else {
            printf("new client fd %d...\n", client_s);

            /* Create a child thread --------------------------------------- */
            qthread_create ( /* Create a child thread */
                my_thread,             /* Thread routine               */
                (void*)client_s);      /* Arguments to be passed       */
         }
    }
 
    close (server_s);           /* close the primary socket */
    return (0);                 /* return code from "main" */
}

int main(int argc, char **argv)
{
    /* local variables for socket connection -------------------------------- */
    unsigned int          server_s;               // Server socket descriptor
    struct sockaddr_in    server_addr;            // Server Internet address
    unsigned int    client_s;       /* Client socket descriptor */
 
 
    /* create a new socket -------------------------------------------------- */
    server_s = socket(AF_INET, SOCK_STREAM, 0);

    /* reuse the server address in case its in a CLOSEWAIT state */
    int enable = 1;
    if (setsockopt(server_s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    int port_num = 8080;
    if (argc > 1)
        port_num = atoi(argv[1]);

    /* fill-in address information, and then bind it ------------------------ */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));
 
    /* Listen for connections and then accept ------------------------------- */
    listen(server_s, PEND_CONNECTIONS);

    qthread_create(main_thread, &server_s);
    qthread_run();
    close(server_s);
}
