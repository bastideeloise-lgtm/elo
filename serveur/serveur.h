#ifndef SERVEUR_H
#define SERVEUR_H

/* Lance le serveur domotique (port 5000).
   ip_simdom / port_simdom : adresse du serveur integre a SimDom. */
void lancer_serveur(const char *ip_simdom, int port_simdom);

#endif
