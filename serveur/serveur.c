
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "serveur.h"

#define PORT 5000
#define TAILLE_BUFFER 1024

void lancer_serveur(void)
{
    WSADATA donnees_winsock;
    SOCKET serveur_socket;
    SOCKET client_socket;

    struct sockaddr_in adresse_serveur;
    struct sockaddr_in adresse_client;

    int taille_adresse = sizeof(adresse_client);
    char message[TAILLE_BUFFER];

    // 1. Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &donnees_winsock) != 0)
    {
        printf("Erreur initialisation Winsock\n");
        return;
    }

    // 2. Creation de la socket
    serveur_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serveur_socket == INVALID_SOCKET)
    {
        printf("Erreur creation socket\n");
        WSACleanup();
        return;
    }

    // 3. Configuration de l'adresse
    memset(&adresse_serveur, 0, sizeof(adresse_serveur));

    adresse_serveur.sin_family = AF_INET;
    adresse_serveur.sin_addr.s_addr = INADDR_ANY;
    adresse_serveur.sin_port = htons(PORT);

    // 4. Association au port
    if (bind(serveur_socket,
             (struct sockaddr *)&adresse_serveur,
             sizeof(adresse_serveur)) == SOCKET_ERROR)
    {
        printf("Erreur bind : %d\n", WSAGetLastError());

        closesocket(serveur_socket);
        WSACleanup();
        return;
    }

    // 5. Mise en ecoute
    if (listen(serveur_socket, 5) == SOCKET_ERROR)
    {
        printf("Erreur listen\n");

        closesocket(serveur_socket);
        WSACleanup();
        return;
    }

    printf("\n====================================\n");
    printf("       SERVEUR DOMOTIQUE ESGI\n");
    printf("====================================\n");
    printf("Serveur demarre sur le port %d\n", PORT);
    printf("En attente de clients...\n");

    // 6. Boucle permanente du serveur
    while (1)
    {
        client_socket = accept(
            serveur_socket,
            (struct sockaddr *)&adresse_client,
            &taille_adresse
        );

        if (client_socket == INVALID_SOCKET)
        {
            printf("Erreur connexion client\n");
            continue;
        }

        printf("\nUn client est connecte !\n");

        // 7. Reception du message
        int octets_recus = recv(
            client_socket,
            message,
            TAILLE_BUFFER - 1,
            0
        );

        if (octets_recus > 0)
        {
            message[octets_recus] = '\0';

            printf("Commande recue : %s\n", message);

            // 8. Traitement provisoire
            const char *reponse = "PONG";

            send(
                client_socket,
                reponse,
                (int)strlen(reponse),
                0
            );
        }
        else
        {
            printf("Aucun message recu\n");
        }

        // 9. Fermeture de la connexion client
        closesocket(client_socket);

        printf("Client deconnecte\n");
    }

    // Fermeture du serveur (non atteinte dans cette version)
    closesocket(serveur_socket);
    WSACleanup();
}
