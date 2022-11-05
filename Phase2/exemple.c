#include "dsm.h"
#include "common_impl.h"

int main(int argc, char **argv)
{
   char *pointer;
   char *current;
   int value;

   pointer = dsm_init(argc,argv);
   current = pointer;

   my_fprintf(stdout, GREEN, DARKNESS, "[%i] Coucou, mon adresse de base est : %p", DSM_NODE_ID, pointer);

   if (DSM_NODE_ID == 0)
     {
       current += 4*sizeof(int);
       *((int *)current) = 2022;
       value = *((int *)current);
       my_fprintf(stdout, GREEN, BOLD, "[%i] valeur de l'entier : %i", DSM_NODE_ID, value);
     }
   else if (DSM_NODE_ID == 1)
     {
       //current += PAGE_SIZE;
       current += 4*sizeof(int);

       value = *((int *)current);
       my_fprintf(stdout, GREEN, BOLD, "[%i] valeur de l'entier : %i", DSM_NODE_ID, value);
     }
   dsm_finalize();
   return 1;
}
