#include <stdio.h>
#include <stdlib.h>
#include "serveur.h"

/* Utilisation :  Serveur.exe [port_simdom] [ip_simdom]
   Exemple     :  Serveur.exe 64705 127.0.0.1                    */
int main(int argc, char *argv[])
{
   int port = 64705;              /* a remplacer par le port affiche par SimDom */
   const char *ip = "127.0.0.1";

   if (argc >= 2) port = atoi(argv[1]);
   if (argc >= 3) ip = argv[2];

   printf("Lancement du projet domotique...\n");
   lancer_serveur(ip, port);
   return 0;
}
