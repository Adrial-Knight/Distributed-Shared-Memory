#include "dsm.h"
#include "common_impl.h"

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */
char* req_str[] = {"DSM_NO_TYPE", "DSM_REQ", "DSM_PAGE", "DSM_CHANGE_PAGE", "DSM_ERROR", "DSM_FINALIZE"};
char* acc_str[] = {"NO_ACCESS", "READ_ACCESS", "WRITE_ACCESS", "READ_WRITE_ACCESS", "UNKNOWN_ACCESS"};

dsm_proc_conn_t* dsm_bro = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* indique l'adresse de debut de la page de numero numpage */
static char* num2address(int numpage){
   char *pointer = (char *)(BASE_ADDR+(numpage*(PAGE_SIZE)));

   if( pointer >= (char *)TOP_ADDR ){
      fprintf(stderr,"[%i] Invalid address !\n", DSM_NODE_ID);
      return NULL;
   }
   else return pointer;
}

/* cette fonction permet de recuperer un numero de page */
/* a partir  d'une adresse  quelconque */
static int address2num(char* addr){
  return (((intptr_t)(addr - BASE_ADDR))/(PAGE_SIZE));
}

/* cette fonction permet de recuperer l'adresse d'une page */
/* a partir d'une adresse quelconque (dans la page)        */
static char* address2pgaddr(char* addr){
  return  (char *)(((intptr_t) addr) & ~(PAGE_SIZE-1));
}

/* met à jour la table des pages */
static void dsm_change_info(int numpage, dsm_page_state_t state, dsm_page_owner_t owner){
   if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {
	if (state != NO_CHANGE )
	table_page[numpage].status = state;
      if (owner >= 0 )
	table_page[numpage].owner = owner;
      return;
   }
   else {
	fprintf(stderr,"[%i] Invalid page number !\n", DSM_NODE_ID);
      return;
   }
}

/* renvoie le rang du proprietaire d'une page */
static dsm_page_owner_t get_owner(int numpage){
   return table_page[numpage].owner;
}

/* renvoie les droits sur une page */
static dsm_page_state_t get_status(int numpage){
   return table_page[numpage].status;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page(int numpage){
   char *page_addr = num2address( numpage );
   mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   return ;
}

/* Changement de la protection d'une page */
static void dsm_protect_page(int numpage, int prot){
    char *page_addr = num2address( numpage );
    if (-1 == mprotect(page_addr, PAGE_SIZE, prot)){
        perror("mprotect");
    }
    return;
}

static void dsm_free_page(int numpage){
   char *page_addr = num2address( numpage );
   munmap(page_addr, PAGE_SIZE);
   return;
}

/* un processus frère me demande une page */
static void proc_req_my_page(char* header, dsm_req_t req){
    int fd_bro = dsm_bro[req.source].fd;
    int page = req.page_num;
    dsm_page_info_t info = {get_status(page), get_owner(page)};
    int i;
    if(info.owner == DSM_NODE_ID){ // c'est bien moi qui possède la page, je l'envoie
        my_fprintf(stdout, CYAN, BOLD, "%s Je donne la page %d à %d", header, page, req.source);

        /* je notifie le processus frère que la page est libre et qu'il va la recevoir*/
        req.source = DSM_NODE_ID;
        req.type = DSM_PAGE;
        write_in_socket(fd_bro, &req, sizeof(dsm_req_t));

        /* j'envoie de la page demandée */
        write_in_socket(fd_bro, num2address(page), PAGE_SIZE);

        /* je régularise ma mémoire */
        read_from_socket(fd_bro, &req, sizeof(dsm_req_t));
        read_from_socket(fd_bro, &info, sizeof(dsm_page_info_t));
        dsm_change_info(req.page_num, info.status, info.owner);
        dsm_free_page(page);
    }
    else{ // ce n'est pas moi qui possède la page
        /* j'envoie le rang du processus qui je pense être le proprietaire */
        my_fprintf(stdout, RED, NO_EFFECT, "%s %d me demande la page %d, mais je ne l'ai pas...", header, req.source, page);
        req.source = DSM_NODE_ID;
        req.type = DSM_ERROR;
        write_in_socket(fd_bro, &req, sizeof(dsm_req_t));
        write_in_socket(fd_bro, &info, sizeof(dsm_page_info_t));
    }
}

/* recevoir les demandes extérieurs */
static void *dsm_comm_daemon(void *arg){
    char* header = malloc(16*sizeof(char));
    sprintf(header, "[Daemon %i]", DSM_NODE_ID);
    int active_bro = DSM_NODE_NUM-1;
    int i;
    dsm_req_t req;
    dsm_page_info_t info;
    struct pollfd poll_fds[DSM_NODE_NUM];

    for (i = 0; i < DSM_NODE_NUM; i++){
        poll_fds[i].fd      = dsm_bro[i].fd;
        poll_fds[i].events  = POLLIN | POLLHUP;
        poll_fds[i].revents = 0;
    }

    while(active_bro > 0){
        if (-1 == poll(poll_fds, DSM_NODE_NUM+1, -1)){
            if(errno != EINTR){ // EINTR: un signal a été envoyé pendant l'éxecution de poll
                perror("[Comm daemon] Poll");
                goto end;
            }
        }
        pthread_mutex_lock(&mutex);
        for (i=0; i < DSM_NODE_NUM; i++){
            if ((poll_fds[i].revents & POLLHUP) || (poll_fds[i].revents & POLLOUT) | (poll_fds[i].revents & POLLPRI)){ // un processus s'est déconnecté
                my_fprintf(stderr, RED, NO_EFFECT, "%s %d s'est deconnecté", header, dsm_bro[i].rank);
                goto end;
            }
            if (poll_fds[i].revents & POLLIN){ // on a reçu une requête
                read_from_socket(poll_fds[i].fd, &req, sizeof(dsm_req_t));
                my_fprintf(stdout, GREY, NO_EFFECT, "%s %d a une requête (%s)", header, dsm_bro[i].rank, req_str[req.type+1]);
                switch(req.type){
                    case DSM_REQ:
                        proc_req_my_page(header, req);
                        break;
                    case DSM_CHANGE_PAGE:
                        read_from_socket(poll_fds[i].fd, &info, sizeof(dsm_page_info_t));
                        dsm_change_info(req.page_num, info.status, info.owner);
                        my_fprintf(stdout, BLUE, NO_EFFECT, "%s %d possède la page %d en %s", header, info.owner, req.page_num, acc_str[info.status]);
                        break;
                    case DSM_PAGE:
                        my_fprintf(stdout, RED, BLINK, "%s /!\\ J'ai lu une requête de type DSM_PAGE !!", header);
                        break;
                    case DSM_FINALIZE:
                        active_bro--;
                        my_fprintf(stdout, YELLOW, NO_EFFECT, "%s %d est prêt à s'arrêter (restant: %i)", header, req.source, active_bro);
                        break;
                }
                poll_fds[i].revents = 0;
            }
            if(poll_fds[i].revents & POLLNVAL){
                my_fprintf(stderr, RED, 1, "%s /!\\ impossible de communiquer avec %d", header, dsm_bro[i].rank);
                goto end;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    end:
        free(header);
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
}

/* fonction exécutée par le traitant si un sgv concerne une des pages */
static void dsm_handler(int page, dsm_access_t access){
    /* variables utiles et affichage console */
    char* header = malloc(16*sizeof(char));
    sprintf(header, "[Traitant %i]", DSM_NODE_ID);
    dsm_page_info_t info = {get_status(page), get_owner(page)};
    my_fprintf(stderr, RED, BOLD, "%s Je n'ai pas de %s sur %i", header, acc_str[access], page);

    info.status = access;
    dsm_req_t req = {DSM_NODE_ID, page, DSM_REQ};
    int i;


    if (info.owner == DSM_NODE_ID){ // je possède la page mais pas avec les bons droits
        info.status = READ_WRITE;
    }
    else{ // je ne possède pas la page
        info.status = access;
        dsm_req_t req = {DSM_NODE_ID, page, DSM_REQ};
        pthread_mutex_lock(&mutex); // j'interrompts mon daemon de communication
            /* j'attents de la notification me prévenant que je vais recevoir la page */
            while(req.type != DSM_PAGE){ // tant que je n'ai pas reçu la bonne notification
            write_in_socket(dsm_bro[info.owner].fd, &req, sizeof(dsm_req_t));
            read_from_socket(dsm_bro[info.owner].fd, &req, sizeof(dsm_req_t));
            if(req.type == DSM_ERROR){
                read_from_socket(dsm_bro[info.owner].fd, &info, sizeof(dsm_page_info_t));
                dsm_change_info(page, info.status, info.owner); // correction de ma table
                my_fprintf(stderr, RED, BOLD, "%s Apparemment ma table n'est pas à jour: je demande à %d", header, info.owner);
                req.type = DSM_REQ;
                req.source = DSM_NODE_ID;
                info.status = access;
                pthread_mutex_unlock(&mutex); // relance mon daemon de communication
                sleep(1);   // le daemon reprend la main pour débloquer la situation
                pthread_mutex_lock(&mutex); // j'interrompts mon daemon de communication
            }
        }

        /* je reçois la page, je la stocke et je mets le bon accès */
        dsm_alloc_page(page);
        read_from_socket(dsm_bro[info.owner].fd, num2address(page), PAGE_SIZE);
    }

    dsm_protect_page(page, info.status);

    /* j'actualise ma table d'informations */
    dsm_change_info(page, info.status, DSM_NODE_ID);
    info.status = get_status(page);
    info.owner = get_owner(page);

    /* je previens les autres processus */
    dsm_req_t resp = {DSM_NODE_ID, page, DSM_CHANGE_PAGE};
    for(i = 0; i < DSM_NODE_NUM; i++){
        if(dsm_bro[i].fd != dsm_bro[DSM_NODE_ID].fd){
            write_in_socket(dsm_bro[i].fd, &resp, sizeof(dsm_req_t));
            write_in_socket(dsm_bro[i].fd, &info, sizeof(dsm_page_info_t));
        }
    }

    pthread_mutex_unlock(&mutex); // je réactive mon daemon de communication
}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context){
    /* adresse qui a provoqué un segv */
    void  *addr = info->si_addr;

    /* accès necessaire pour lever le segv */
    dsm_access_t access = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;

    /* adresse de la page dont fait partie l'adresse qui a provoqué la faute */
    void* page_addr = (void *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

    if ((addr >= (void *)BASE_ADDR) && (addr < (void *)TOP_ADDR)){
        dsm_handler(address2num(addr), access);
    }
    else{
        my_fprintf(stderr, RED, BLINK, "SEG FAULT HORS PLAGE (%p)", page_addr);
        dsm_finalize();
        abort();
    }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char *argv[]){
    struct sigaction act;
    int index;
    char port_str[8];
    int i, j;

    /* Récupération de la valeur des variables d'environnement */
    /* DSMEXEC_FD et MASTER_FD                                 */
    int master_fd  = atoi(getenv(MASTER_FD));
    int dsmexec_fd = atoi(getenv(DSMEXEC_FD));

    /* reception du nombre de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_NUM) */
    DSM_NODE_NUM = read_int_size(dsmexec_fd);
    dsm_bro = (dsm_proc_conn_t*) malloc((DSM_NODE_NUM) * sizeof(dsm_proc_conn_t));

    /* reception de mon numero de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_ID)      */
    DSM_NODE_ID = read_int_size(dsmexec_fd);

    /* reception des informations de connexion des autres */
    /* processus envoyees par le lanceur :                */
    /* nom de machine, numero de port, etc.               */
    for (i = 0; i < DSM_NODE_NUM; i++) {
        read_from_socket(dsmexec_fd, &dsm_bro[i], sizeof(struct dsm_proc_conn));
    }
    /* le lanceur n'a plus rien à nous envoyer, et vice-versa */
    close(dsmexec_fd);

    /* Prépapration de la socket master_fd pour recevoir les connexions*/
    listen(master_fd, DSM_NODE_NUM-DSM_NODE_ID+1);

    /* initialisation des connexions              */
    /* avec les autres processus : connect/accept */
    for(j = 0; j < DSM_NODE_NUM; j++){
        if(DSM_NODE_ID == j){
            dsm_bro[DSM_NODE_ID].fd = master_fd;
            for (i = j+1; i < DSM_NODE_NUM; i++){
                memset(port_str, 0, 8);
                sprintf(port_str, "%d", dsm_bro[i].port_num);
                while(-1 == (dsm_bro[i].fd = socket_and_connect(dsm_bro[i].machine, port_str))){}
            }
        }
        else if(DSM_NODE_ID > j){
            dsm_bro[j].fd = accept_client(master_fd);
        }
    }

    /* Allocation des pages en tourniquet */
    for(index = 0; index < PAGE_NUMBER; index ++){
        if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
        dsm_alloc_page(index);
        dsm_change_info( index, WRITE_ONLY, index % DSM_NODE_NUM);
    }

    /* mise en place du traitant de SIGSEGV */
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = segv_handler;
    sigaction(SIGSEGV, &act, NULL);

    /* creation du thread de communication           */
    /* ce thread va attendre et traiter les requetes */
    /* des autres processus                          */
    pthread_create(&comm_daemon, NULL, dsm_comm_daemon, NULL);

    /* Adresse de début de la zone de mémoire partagée */
    return ((char *)BASE_ADDR);
}

void dsm_finalize(){
    int i;
    dsm_req_t final = {DSM_NODE_ID, -1, DSM_FINALIZE};
    sleep(1); // sans cela se termine, mais des erreurs sont affichées par un des daemons

    /* on signale aux autres processus que l'on est prêt à terminer */
    pthread_mutex_lock(&mutex);
    for(i = 0; i < DSM_NODE_NUM; i++){
        if(dsm_bro[i].fd != dsm_bro[DSM_NODE_ID].fd)
            write_in_socket(dsm_bro[i].fd, &final, sizeof(dsm_req_t));
    }
    pthread_mutex_unlock(&mutex);

    /* on termine correctement le thread de communication */
    pthread_join(comm_daemon, NULL);

    /* on ferme proprement les connexions avec les autres processus */
    for(i = 0; i < DSM_NODE_NUM; i++){
        close(dsm_bro[i].fd);
    }

    /* on libère les pages que l'on possède */
    for(i = 0; i < PAGE_NUMBER; i++){
        if (get_owner(i) == DSM_NODE_ID)
            dsm_free_page(i);
    }

    pthread_mutex_destroy(&mutex);
    free(dsm_bro);

    my_fprintf(stdout, GREEN, ITALIC, "Fin du processus %i", DSM_NODE_ID);
    return;
}
