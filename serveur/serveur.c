#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "serveur.h"

#define PORT_SERVEUR 5000
#define TAILLE       4096
#define MAX_EQUIP    64

/* ------------------------------------------------------------------ */
/*  Equipements (en memoire pour l'instant, MySQL viendra ensuite)     */
/* ------------------------------------------------------------------ */
typedef struct {
    int  id;
    char etage[32];
    char piece[32];
    char nom[48];
    char ip[32];       /* adresse IP de l'automate (SimDom > Equipements > Configuration) */
    char entree[16];   /* entree de l'automate                                           */
    int  etat;         /* 0 = eteint, 1 = allume, -1 = inconnu                           */
} Equipement;

/* ATTENTION : IP / entrees = EXEMPLES. Remplace-les par les vraies valeurs
   lues dans SimDom (menu Equipements > Configuration).                    */
static Equipement g_equip[MAX_EQUIP] = {
    {1, "Exterieur",       "Entree_ouest", "Hublot",          "192.168.0.100", "00010010", 0},
    {2, "Exterieur",       "Entree_ouest", "Spots_x2",        "192.168.0.100", "00010011", 0},
    {3, "Rez_de_chaussee", "Cuisine",      "Lumiere_plafond", "192.168.0.101", "00000001", 0},
    {4, "Premier_etage",   "Chambre",      "Lumiere_plafond", "192.168.0.102", "00000001", 0},
};
static int g_nb_equip = 4;

static char g_simdom_ip[64] = "127.0.0.1";
static int  g_simdom_port   = 64705;

/* ------------------------------------------------------------------ */
/*  Outils                                                             */
/* ------------------------------------------------------------------ */

/* Ajoute du texte formate a la fin d'un buffer (sans depasser) */
static void ajouter(char *out, int taille, const char *fmt, ...)
{
    int len = (int)strlen(out);
    va_list ap;
    if (len >= taille - 1) return;
    va_start(ap, fmt);
    vsnprintf(out + len, taille - len, fmt, ap);
    va_end(ap);
}

/* Lit une ligne (jusqu'a '\n') envoyee par un client */
static int lire_ligne(SOCKET s, char *buf, int max)
{
    int n = 0;
    char c;
    while (n < max - 1) {
        int r = recv(s, &c, 1, 0);
        if (r <= 0) break;
        if (c == '\n') break;
        if (c != '\r') buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

static Equipement *trouver_equip(int id)
{
    int i;
    for (i = 0; i < g_nb_equip; i++)
        if (g_equip[i].id == id) return &g_equip[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Communication avec SimDom  ("<ip> <entree> <0|1|10>")             */
/* ------------------------------------------------------------------ */
static int envoyer_simdom(const Equipement *e, const char *valeur, char *rep, int taille)
{
    SOCKET s;
    struct sockaddr_in a;
    char msg[128];
    DWORD delai = 2000;
    int n;

    rep[0] = '\0';
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((u_short)g_simdom_port);
    if (inet_pton(AF_INET, g_simdom_ip, &a.sin_addr) != 1) { closesocket(s); return -1; }

    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&delai, sizeof(delai));

    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    snprintf(msg, sizeof(msg), "%s %s %s\n", e->ip, e->entree, valeur);
    printf("   -> SimDom : %s", msg);
    send(s, msg, (int)strlen(msg), 0);

    n = recv(s, rep, taille - 1, 0);
    if (n < 0) n = 0;
    rep[n] = '\0';
    printf("   <- SimDom : [%s]\n", rep);

    closesocket(s);
    return 0;
}

/* Interprete la reponse a la commande "10" (lecture d'etat).
   A VERIFIER avec Wireshark : on suppose que le dernier mot vaut 0 ou 1. */
static int interpreter_etat(const char *rep)
{
    char copie[256];
    char *p;
    int len;

    strncpy(copie, rep, sizeof(copie) - 1);
    copie[sizeof(copie) - 1] = '\0';
    len = (int)strlen(copie);
    while (len > 0 && (copie[len-1] == '\n' || copie[len-1] == '\r' || copie[len-1] == ' '))
        copie[--len] = '\0';

    p = strrchr(copie, ' ');
    p = p ? p + 1 : copie;

    if (strcmp(p, "1") == 0) return 1;
    if (strcmp(p, "0") == 0) return 0;
    return -1;
}

static void rafraichir_equip(Equipement *e)
{
    char rep[256];
    if (envoyer_simdom(e, "10", rep, sizeof(rep)) == 0)
        e->etat = interpreter_etat(rep);
}

/* ------------------------------------------------------------------ */
/*  Protocole client <-> serveur (texte, une ligne par requete)        */
/*    PING                       -> OK PONG                            */
/*    ETAGES                     -> OK etage1;etage2;...               */
/*    PIECES <etage>             -> OK piece1;piece2;...               */
/*    EQUIPEMENTS <etage> <piece>-> OK id,nom,etat;id,nom,etat;...     */
/*    RAFRAICHIR <etage> <piece> -> idem, apres lecture dans SimDom    */
/*    ALLUMER <id> / ETEINDRE <id> -> OK  ou  ERR ...                  */
/* ------------------------------------------------------------------ */
static void liste_equipements(const char *etage, const char *piece, int rafraichir,
                              char *out, int taille)
{
    int i, premier = 1;
    strcpy(out, "OK ");
    for (i = 0; i < g_nb_equip; i++) {
        Equipement *e = &g_equip[i];
        if (strcmp(e->etage, etage) == 0 && strcmp(e->piece, piece) == 0) {
            if (rafraichir) rafraichir_equip(e);
            ajouter(out, taille, "%s%d,%s,%d", premier ? "" : ";", e->id, e->nom, e->etat);
            premier = 0;
        }
    }
}

static void traiter_commande(const char *ligne, char *out, int taille)
{
    char cmd[32] = "", a1[64] = "", a2[64] = "";
    int i, j;

    out[0] = '\0';
    sscanf(ligne, "%31s %63s %63s", cmd, a1, a2);

    if (strcmp(cmd, "PING") == 0) {
        strcpy(out, "OK PONG");
    }
    else if (strcmp(cmd, "ETAGES") == 0) {
        int premier = 1;
        strcpy(out, "OK ");
        for (i = 0; i < g_nb_equip; i++) {
            int deja = 0;
            for (j = 0; j < i; j++)
                if (strcmp(g_equip[j].etage, g_equip[i].etage) == 0) deja = 1;
            if (!deja) {
                ajouter(out, taille, "%s%s", premier ? "" : ";", g_equip[i].etage);
                premier = 0;
            }
        }
    }
    else if (strcmp(cmd, "PIECES") == 0) {
        int premier = 1;
        strcpy(out, "OK ");
        for (i = 0; i < g_nb_equip; i++) {
            int deja = 0;
            if (strcmp(g_equip[i].etage, a1) != 0) continue;
            for (j = 0; j < i; j++)
                if (strcmp(g_equip[j].etage, a1) == 0 &&
                    strcmp(g_equip[j].piece, g_equip[i].piece) == 0) deja = 1;
            if (!deja) {
                ajouter(out, taille, "%s%s", premier ? "" : ";", g_equip[i].piece);
                premier = 0;
            }
        }
    }
    else if (strcmp(cmd, "EQUIPEMENTS") == 0) {
        liste_equipements(a1, a2, 0, out, taille);
    }
    else if (strcmp(cmd, "RAFRAICHIR") == 0) {
        liste_equipements(a1, a2, 1, out, taille);
    }
    else if (strcmp(cmd, "ALLUMER") == 0 || strcmp(cmd, "ETEINDRE") == 0) {
        Equipement *e = trouver_equip(atoi(a1));
        int allumer = (strcmp(cmd, "ALLUMER") == 0);
        char rep[256];

        if (!e) {
            strcpy(out, "ERR equipement inconnu");
        } else if (envoyer_simdom(e, allumer ? "1" : "0", rep, sizeof(rep)) != 0) {
            strcpy(out, "ERR SimDom injoignable");
        } else {
            e->etat = allumer ? 1 : 0;
            strcpy(out, "OK");
        }
    }
    else {
        strcpy(out, "ERR commande inconnue");
    }
}

/* ------------------------------------------------------------------ */
/*  Boucle principale du serveur                                       */
/* ------------------------------------------------------------------ */
void lancer_serveur(const char *ip_simdom, int port_simdom)
{
    WSADATA wsa;
    SOCKET srv, cli;
    struct sockaddr_in adr, adr_cli;
    int taille_adr = sizeof(adr_cli);
    int oui = 1;
    char requete[TAILLE], reponse[TAILLE];

    strncpy(g_simdom_ip, ip_simdom, sizeof(g_simdom_ip) - 1);
    g_simdom_port = port_simdom;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Erreur : Winsock\n");
        return;
    }

    srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { printf("Erreur socket\n"); WSACleanup(); return; }
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&oui, sizeof(oui));

    memset(&adr, 0, sizeof(adr));
    adr.sin_family      = AF_INET;
    adr.sin_addr.s_addr = INADDR_ANY;
    adr.sin_port        = htons(PORT_SERVEUR);

    if (bind(srv, (struct sockaddr *)&adr, sizeof(adr)) == SOCKET_ERROR) {
        printf("Erreur bind : %d\n", WSAGetLastError());
        closesocket(srv); WSACleanup(); return;
    }
    if (listen(srv, 5) == SOCKET_ERROR) {
        printf("Erreur listen\n");
        closesocket(srv); WSACleanup(); return;
    }

    printf("====================================\n");
    printf(" SERVEUR DOMOTIQUE DEMARRE\n");
    printf(" Ecoute sur le port %d\n", PORT_SERVEUR);
    printf(" SimDom : %s:%d\n", g_simdom_ip, g_simdom_port);
    printf("====================================\n");

    for (;;) {
        cli = accept(srv, (struct sockaddr *)&adr_cli, &taille_adr);
        if (cli == INVALID_SOCKET) continue;

        if (lire_ligne(cli, requete, sizeof(requete)) > 0) {
            printf("[CLIENT %s] %s\n", inet_ntoa(adr_cli.sin_addr), requete);
            traiter_commande(requete, reponse, sizeof(reponse));
            printf("[REPONSE ] %s\n", reponse);
            ajouter(reponse, sizeof(reponse), "\n");
            send(cli, reponse, (int)strlen(reponse), 0);
        }
        closesocket(cli);
    }
}
