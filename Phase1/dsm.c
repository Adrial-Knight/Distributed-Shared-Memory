#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "common_impl.h"

int main(int argc, char *argv[]){
    dsm_proc_conn_t* dsm_array = NULL;
    int num_procs = -1;
    int master_fd  = atoi(getenv(MASTER_FD));
    int dsmexec_fd = atoi(getenv(DSMEXEC_FD));
    int* dsm_bro  = NULL;
    int rank = -1;
    char buff[2048];
    char port_str[8];
    int i, j;

    num_procs = read_int_size(dsmexec_fd);
    dsm_array = malloc(num_procs * sizeof(dsm_proc_conn_t));
    dsm_bro   = malloc(num_procs * sizeof(int));

    rank = read_int_size(dsmexec_fd);

    /* réception des informations pour les connexions entre tous les processus*/
    for (i = 0; i < num_procs; i++) {
        read_from_socket(dsmexec_fd, &dsm_array[i], sizeof(struct dsm_proc_conn));
    }
    /* connexion aux processes frères */

    for(j = 0; j < num_procs; j++){
        if(rank == j){
            dsm_bro[j] = -1;
            for (i = j+1; i < num_procs; i++){
                memset(port_str, 0, 8);
                sprintf(port_str, "%d", dsm_array[dsm_array[i].rank].port_num);

                dsm_bro[i] = socket_and_connect(dsm_array[dsm_array[i].rank].machine, port_str);

                if(VERBOSE){
                    memset(buff, 0, 2048);
                    if(rank < 10)
                        sprintf(buff, "0%d\n", rank);
                    else
                        sprintf(buff, "%d\n", rank);
                    write_in_socket(dsm_bro[i], buff, strlen(buff)+1);
                    memset(buff, 0, 2048);
                    read_from_socket(dsm_bro[i], buff, 4);
                    write(STDOUT_FILENO, buff, 4);
                }

            }
        }
        else if(rank > j){

            dsm_bro[j] = accept_client(master_fd);

            if(VERBOSE){
                memset(buff, 0, 2048);
                read_from_socket(dsm_bro[j], buff, 4);
                write(STDOUT_FILENO, buff, 4);
                memset(buff, 0, 2048);
                if(rank < 10)
                    sprintf(buff, "0%d\n", rank);
                else
                    sprintf(buff, "%d\n", rank);
                write_in_socket(dsm_bro[j], buff, strlen(buff)+1);
            }

        }
    }

    for (i = 0; i < num_procs; i++) {
        close(dsm_bro[i]);
    }

    return 0;
}
