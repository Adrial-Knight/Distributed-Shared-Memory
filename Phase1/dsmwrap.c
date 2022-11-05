#include "common_impl.h"

int main(int argc, char **argv){
   /* processus intermediaire pour "nettoyer" */
   /* la liste des arguments qu'on va passer */
   /* a la commande a executer finalement  */
   char* exec_hostname = argv[1];
   char* exec_port = argv[2];
   int num_procs = atoi(argv[3]);
   int exec_sock = -1;

   char hostname[MAX_STR];
   int hostname_size = -1;
   pid_t pid;

   int wrap_sock = -1;
   int wrap_port = -1;
   char sock_str[8] = {0};

   int rank = atoi(argv[4]);

   char** dsm_argv = NULL;
   int dsm_argc = argc -5 +1;
   char path[128] = {0};
   int i;

   /* creation d'une socket pour se connecter au */
   /* au lanceur et envoyer/recevoir les infos */
   /* necessaires pour la phase dsm_init */
   while(-1 == (exec_sock = socket_and_connect(exec_hostname, exec_port))){}

   /* Envoi du nom de machine au lanceur */
   gethostname(hostname, MAX_STR);
   hostname_size = strlen(hostname);
   write_int_size(exec_sock, &hostname_size);
   write_in_socket(exec_sock, hostname, hostname_size);


   /* Envoi du pid au lanceur (optionnel) */
   pid = getpid();
   write_int_size(exec_sock, &pid);

   /* Creation de la socket d'ecoute pour les */
   /* connexions avec les autres processus dsm */
   wrap_sock = listening_socket(num_procs);

   /* Envoi du numero de port au lanceur */
   /* pour qu'il le propage à tous les autres */
   /* processus dsm */
   wrap_port = get_associated_port(wrap_sock);
   write_int_size(exec_sock, &wrap_port);

   /* renvoie le rang pour éviter des mélanges dans le proc_array de dsmexec
      et donc dans l'affichage du prompt */
   write_int_size(exec_sock, &rank);

   /* on execute la bonne commande */
   /* attention au chemin à utiliser ! */

   dsm_argv = malloc(dsm_argc * sizeof(char*));

   /* construction du chemin vers l'exécutable */
   dsm_argv[0] = malloc(strlen(argv[0]) - strlen("dsmwrap") + strlen(argv[5]));
   memcpy(path, argv[0], strlen(argv[0]) - strlen("dsmwrap"));
   sprintf(dsm_argv[0], "%s%s", path, argv[5]);

   /* on agence les arguments à passer à l'exécutable */
   for(i = 6; i < argc; i++){
       dsm_argv[i - 5] = malloc(sizeof(argv[i]));
       strcpy(dsm_argv[i - 5], argv[i]);
   }

   memset(sock_str, 0, strlen(sock_str));
   sprintf(sock_str, "%d", wrap_sock);
   setenv(MASTER_FD, sock_str, 1);

   memset(sock_str, 0, strlen(sock_str));
   sprintf(sock_str, "%d", exec_sock);
   setenv(DSMEXEC_FD, sock_str, 1);

   dsm_argv[dsm_argc - 1] = NULL;

   /* on execute la commande */
   execvp(dsm_argv[0], dsm_argv);

   /************** ATTENTION **************/
   /* vous remarquerez que ce n'est pas   */
   /* ce processus qui récupère son rang, */
   /* ni le nombre de processus           */
   /* ni les informations de connexion    */
   /* (cf protocole dans dsmexec)         */
   /***************************************/

   return 0;
}
