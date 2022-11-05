#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <err.h>

/* autres includes (eventuellement) */

#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}
#define BUF_SIZE 128
#define MAX_NUM_PROCS 100
#define VERBOSE 1
#define MASTER_FD  "MASTER_FD"
#define DSMEXEC_FD "DSMEXEC_FD"

int socket_and_connect(char *hostname, char *port);
int listening_socket(int num_procs);
int get_associated_port(int sock_fd);
void write_int_size(int fd, void *ptr);
int read_int_size(int fd);
int read_from_socket(int fd, void *buf, int size);
int write_in_socket(int fd, void *buf, int size);
int accept_client(int listening_sock);


/**************************************************************/
/****************** DEBUT DE PARTIE NON MODIFIABLE ************/
/**************************************************************/

#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];

/* definition du type des infos */
/* de connexion des processus dsm */
struct dsm_proc_conn  {
   int      rank;
   maxstr_t machine;
   int      port_num;
   int      fd;
   int      fd_for_exit; /* special */
};

typedef struct dsm_proc_conn dsm_proc_conn_t;

/**************************************************************/
/******************* FIN DE PARTIE NON MODIFIABLE *************/
/**************************************************************/

/* definition du type des infos */
/* d'identification des processus dsm */
struct dsm_proc {
  pid_t pid;
  dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;
