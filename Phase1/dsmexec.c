#include "common_impl.h"

typedef struct arg_print{
    dsm_proc_t* proc_machine;
    char std[7];
    int pipe_fd;
}arg_print_t;

/* variables globales */
pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *proc_array = NULL;

/* le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void)
{
  fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
  fflush(stdout);
  exit(EXIT_FAILURE);
}

void sigchld_handler(int sig)
{
   /* on traite les fils qui se terminent */
   /* pour eviter les zombies */
   wait(NULL);
}


dsm_proc_t* read_machine_file(char* path_machine, int* num_procs){
    int fd = -1;

    if(-1 == (fd = open(path_machine, O_RDONLY))){
        perror(path_machine);
        exit(EXIT_FAILURE);
    }

    char buffer;
    int nb_car = 0;
    *num_procs = 0;

    while(read(fd, &buffer, 1)){
        if(buffer == '\n'){
            if(nb_car)
                (*num_procs)++;

            nb_car = 0;
      }
      else
          nb_car ++;
    }

    lseek(fd, 0, SEEK_SET);

    dsm_proc_t* proc_array = malloc(*num_procs*sizeof(dsm_proc_t));

    maxstr_t machine;
    memset(machine, 0, MAX_STR);
    nb_car = 0;
    *num_procs = 0;

    while(read(fd, &buffer, 1)){
        if(buffer == '\n'){
            if (nb_car){
                strcpy(proc_array[*num_procs].connect_info.machine, machine);
                (*num_procs)++;
            }

            memset(machine, 0, MAX_STR);
            nb_car = 0;
        }

        else{
            machine[nb_car] = buffer;
            nb_car++;
        }
    }


    close(fd);
    return proc_array;
}

char** init_ssh_argv(int argc, char** argv, char* path_bin, char* exec_port, int num_procs){
    char** ssh_argv = malloc((7 + argc + 1) * sizeof(char*));

    ssh_argv[0] = "ssh";

    /* ssh_argv[1] est le nom de la machine distante */

    /* chemin vers dsmwrap, lancé avec ssh */
    ssh_argv[2] = malloc(MAX_STR * sizeof(char));
    sprintf(ssh_argv[2], "%s/dsmwrap", path_bin);

    /* hostname du lanceur */
    ssh_argv[3] = malloc(MAX_STR * sizeof(char));
    gethostname(ssh_argv[3], MAX_STR);

    /* port d'écoute du lanceur */
    ssh_argv[4] = exec_port;

    /* nombre de processus lancés*/
    ssh_argv[5] = malloc(8 * sizeof(char));
    sprintf(ssh_argv[5], "%d", num_procs);

    /* ssh_argv[6] est le rang */
    ssh_argv[6] = malloc(8 * sizeof(char));

    /* exécutable final à lancer, avec ses arguments */
    int i;
    for (i = 0; i < argc; i++) {
        ssh_argv[i + 7] = argv[i + 2];
    }

    ssh_argv[i + 7] = NULL;

    return ssh_argv;
}

void* print_routine(void* arg){
    arg_print_t* arguments = (arg_print_t*)arg;
    maxstr_t machine;
    int rank, pipe_fd;
    char std[7];
    char buffer[BUF_SIZE];
    int header_size;
    char c;
    int ret = 0;
    int nb_car = 0;

    pipe_fd = arguments->pipe_fd;
    rank = arguments->proc_machine->connect_info.rank;
    strcpy(machine, arguments->proc_machine->connect_info.machine);
    strcpy(std, arguments->std);

    /* construction du header [Proc xx : machine : stdxxx] */
    header_size = 15 + strlen(machine) + strlen(std);

    int tmp_rank = rank; // taille de rank en base 10 dans le header
    do {
        header_size++;
        tmp_rank = tmp_rank/10;
    } while(tmp_rank > 10);

    char header[header_size];

    sprintf(header, "[Proc %d : %s : %s] ", rank, machine, std);

    while(1){
        ret = read(pipe_fd, &c, 1);
        if (!ret) break; // le tube vient d'être fermé en écriture
        if (ret > 0 && nb_car < BUF_SIZE){
            buffer[nb_car] = c;
            nb_car++;
        }
        if(c == '\n' || nb_car == BUF_SIZE){
            pthread_mutex_lock(&verrou);
            fprintf(stdout, "%s%s", header, buffer);
            if(nb_car == BUF_SIZE)
                write(STDOUT_FILENO, (const void*)"\n", 1);

            pthread_mutex_unlock(&verrou);
            memset(buffer, 0, BUF_SIZE);
            nb_car = 0;
        }
    }

    pthread_exit(NULL);
}


void remove_zombies(){
    struct sigaction remove_zombies;
    remove_zombies.sa_flags = SA_NOCLDWAIT;
    memset(&remove_zombies, 0, sizeof(struct sigaction));
    remove_zombies.sa_handler = sigchld_handler;
    if (-1 == sigaction(SIGCHLD, &remove_zombies, NULL)) {
        ERROR_EXIT("Error sigaction");
    }
}

/*void children_create( int num_procs, char* path_bin, char** ssh_argv, int fd_pipe_stdout[num_procs][2], int fd_pipe_stderr[num_procs][2], char* argv[]){}*/

void dsm_read_info(int num_procs){
    int i;
    int str_size;
    int wrap_fd;

    for(i = 0; i < num_procs ; i++){
        wrap_fd = proc_array[i].connect_info.fd;

        /*  On recupere le nom de la machine distante */
        /* 1- d'abord la taille de la chaine */
        str_size = read_int_size(wrap_fd);

        /* 2- puis la chaine elle-meme */
        read_from_socket(wrap_fd, proc_array[i].connect_info.machine, str_size);


        /* On recupere le pid du processus distant  (optionnel)*/
        proc_array[i].pid = read_int_size(wrap_fd);

        /* On recupere le numero de port de la socket */
        /* d'ecoute des processus distants */
        /* cf code de dsmwrap.c */
        proc_array[i].connect_info.port_num = read_int_size(wrap_fd);
        proc_array[i].connect_info.rank = read_int_size(wrap_fd);

    }
}

/*void thread_reader(int num_procs, int fd_pipe_stdout[num_procs][2], int fd_pipe_stderr[num_procs][2], pthread_t tid[num_procs][2]){}*/

/*******************************************************/
/*********** ATTENTION : BIEN LIRE LA STRUCTURE DU *****/
/*********** MAIN AFIN DE NE PAS AVOIR A REFAIRE *******/
/*********** PLUS TARD LE MEME TRAVAIL DEUX FOIS *******/
/*******************************************************/

int main(int argc, char *argv[])
{
    if (argc < 3){
        usage();
    }
    else {
        pid_t pid;
        int num_procs;
        int i, j;

        int exec_lst_sock = -1;
        char exec_port[8] = {0};
        char exec_name[MAX_STR];
        gethostname(exec_name, MAX_STR);

        char* path_bin = getenv("DSM_BIN");
        if(path_bin == NULL){
            printf("getenv: DSM_BIN undefined\n");
            exit(EXIT_FAILURE);
        }

        char** ssh_argv = NULL;


        /* Variables pour la connexion des processus distants */
        int wrap_fd;
        struct sockaddr wrap_addr;
        socklen_t size = sizeof(struct sockaddr);

        /* Mise en place d'un traitant pour recuperer les fils zombies*/
        /* XXX.sa_handler = sigchld_handler; */

        remove_zombies();

        /* lecture du fichier de machines */

        proc_array = read_machine_file(argv[1], &num_procs);


        /* 1- on recupere le nombre de processus a lancer */
        /* 2- on recupere les noms des machines : le nom de */
        /* la machine est un des elements d'identification */

        /* creation de la socket d'ecoute */
        if(-1 == (exec_lst_sock = listening_socket(num_procs))){
            ERROR_EXIT("Error listening socket");
        }
        sprintf(exec_port, "%d", get_associated_port(exec_lst_sock));


        /* + ecoute effective */
        if (-1 == listen(exec_lst_sock, num_procs)) {
            perror("Listen");
        }

        /* Initialisation du tableau d'arguments pour le ssh */
        ssh_argv = init_ssh_argv(argc, argv, path_bin, exec_port, num_procs);


        /* creation des fils */
        int fd_pipe_stdout[num_procs][2];
        int fd_pipe_stderr[num_procs][2];


        //children_create(num_procs, path_bin, ssh_argv, fd_pipe_stdout, fd_pipe_stderr, argv);

        /* Initialisation du tableau d'arguments pour le ssh */
        ssh_argv = init_ssh_argv(argc, argv, path_bin, exec_port, num_procs);

        for(i = 0; i < num_procs ; i++) {
            /* creation du tube pour rediriger stdout */
            pipe(&fd_pipe_stdout[i][0]);

            /* creation du tube pour rediriger stderr */
            pipe(&fd_pipe_stderr[i][0]);

            pid = fork();

            if(pid == -1) ERROR_EXIT("fork");

            if (pid == 0) { /* fils */
                /* le rang du processus */
                proc_array[i].connect_info.rank = i;

                /* redirection stdout */
                close(STDOUT_FILENO);
                dup(fd_pipe_stdout[i][1]);

                /* redirection stderr */
                close(STDERR_FILENO);
                dup(fd_pipe_stderr[i][1]);

                /* ferme les extrémités des tubes inutiles */
                close(fd_pipe_stdout[i][0]);
                close(fd_pipe_stdout[i][1]);
                close(fd_pipe_stderr[i][0]);
                close(fd_pipe_stderr[i][1]);

                /* complète le tableau d'arguments pour le ssh */

                /* machine distante ciblée */
                ssh_argv[1] = proc_array[i].connect_info.machine;

                sprintf(ssh_argv[6], "%d", proc_array[i].connect_info.rank);

                if(-1 == execvp("ssh", ssh_argv)){
                    perror("execvp ssh");
                };

            } else  if(pid > 0) { /* père */
                /* fermeture des extremités des tubes non utiles */
                close(fd_pipe_stderr[i][1]);
                close(fd_pipe_stdout[i][1]);

                /* on accepte les connexions des processus dsm */
                while (-1 == (wrap_fd = accept(exec_lst_sock, &wrap_addr, &size))) {}
                proc_array[i].connect_info.fd = wrap_fd;
                if(VERBOSE)
                    printf("%d / %d\n", i+1, num_procs);

                num_procs_creat++;
            }
        }
        dsm_read_info(num_procs);

        /***********************************************************/
        /********** ATTENTION : LE PROTOCOLE D'ECHANGE *************/
        /********** DECRIT CI-DESSOUS NE DOIT PAS ETRE *************/
        /********** MODIFIE, NI DEPLACE DANS LE CODE   *************/
        /***********************************************************/

        /* 1- envoi du nombre de processus aux processus dsm*/
        /* On envoie cette information sous la forme d'un ENTIER */
        /* (IE PAS UNE CHAINE DE CARACTERES */
        for(i = 0; i < num_procs; i++){
            write_int_size(proc_array[i].connect_info.fd, &num_procs);
        }

        /* 2- envoi des rangs aux processus dsm */
        /* chaque processus distant ne reçoit QUE SON numéro de rang */
        /* On envoie cette information sous la forme d'un ENTIER */
        /* (IE PAS UNE CHAINE DE CARACTERES */
        for(i = 0; i < num_procs; i++){
            write_int_size(proc_array[i].connect_info.fd, &proc_array[i].connect_info.rank);
        }

        /* 3- envoi des infos de connexion aux processus */
        /* Chaque processus distant doit recevoir un nombre de */
        /* structures de type dsm_proc_conn_t égal au nombre TOTAL de */
        /* processus distants, ce qui signifie qu'un processus */
        /* distant recevra ses propres infos de connexion */
        /* (qu'il n'utilisera pas, nous sommes bien d'accords). */
        for(i = 0; i < num_procs; i++){
            for(j = 0; j < num_procs; j++){
                write_in_socket(proc_array[i].connect_info.fd, &proc_array[j].connect_info, sizeof(struct dsm_proc_conn));
            }
        }

        /***********************************************************/
        /********** FIN DU PROTOCOLE D'ECHANGE DES DONNEES *********/
        /********** ENTRE DSMEXEC ET LES PROCESSUS DISTANTS ********/
        /***********************************************************/

        /* gestion des E/S : on recupere les caracteres */
        /* sur les tubes de redirection de stdout/stderr */
        pthread_t tid[num_procs][2];

        arg_print_t arg_thread[num_procs][2];
        int rank = -1;

        for(i = 0; i < num_procs; i++){ // arguments des threads
            rank = proc_array[i].connect_info.rank;

            arg_thread[i][0].proc_machine = &proc_array[i];
            arg_thread[i][1].proc_machine = &proc_array[i];

            strcpy(arg_thread[i][0].std, "stdout");
            strcpy(arg_thread[i][1].std, "stderr");

            arg_thread[i][0].pipe_fd = fd_pipe_stdout[rank][0];
            arg_thread[i][1].pipe_fd = fd_pipe_stderr[rank][0];
        }

        for(i = 0; i < num_procs; i++){
            pthread_create(&tid[i][0], NULL, print_routine, (void*)&arg_thread[i][0]);
            pthread_create(&tid[i][1], NULL, print_routine, (void*)&arg_thread[i][1]);
        }
        //thread_reader(num_procs, fd_pipe_stdout, fd_pipe_stderr, tid);

        for(i = 0; i < num_procs; i++){
            pthread_join(tid[i][0], NULL);
            pthread_join(tid[i][1], NULL);
        }


        /* on attend les processus fils */

        /* on ferme les descripteurs proprement */

        /* on ferme la socket d'ecoute */
        close(exec_lst_sock);

        /* on libère la mémoire allouée */
        free(ssh_argv[2]);
        free(ssh_argv[3]);
        free(ssh_argv[5]);
        free(ssh_argv[6]);
        free(ssh_argv);
        free(proc_array);
        pthread_mutex_destroy(&verrou);
    }
    exit(EXIT_SUCCESS);
}
