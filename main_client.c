/* Client domotique - IHM Win32 en C pur
   Menu principal (fig. 2/3 du sujet) + fenetre "Pilotage direct" (fig. 7 a 12) */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define SERVEUR_IP    "127.0.0.1"
#define SERVEUR_PORT  5000
#define MAX_EQ        64

/* Identifiants des controles */
#define ID_BTN_CONNEXION  201
#define ID_BTN_PILOTAGE   202
#define ID_BTN_APROPOS    203
#define ID_COMBO_ETAGE    101
#define ID_COMBO_PIECE    102
#define ID_LISTE          103
#define ID_BTN_RAFRAICHIR 104
#define ID_BTN_TOGGLE     105
#define ID_BTN_FERMER     106

static HINSTANCE g_inst;
static HWND g_menu, g_pilotage;
static HWND g_btnPilotage;

/* Etat de la fenetre de pilotage */
static HWND g_cbEtage, g_cbPiece, g_liste, g_btnToggle, g_btnRaf, g_statut;
static int  g_ids[MAX_EQ];
static int  g_etats[MAX_EQ];
static int  g_nb = 0;

/* ------------------------------------------------------------------ */
/*  Reseau : une requete = une connexion TCP vers notre serveur        */
/* ------------------------------------------------------------------ */
static int requete(const char *req, char *rep, int taille)
{
    SOCKET s;
    struct sockaddr_in a;
    char msg[256];
    int total = 0, n;

    rep[0] = '\0';
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons(SERVEUR_PORT);
    inet_pton(AF_INET, SERVEUR_IP, &a.sin_addr);

    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    snprintf(msg, sizeof(msg), "%s\n", req);
    send(s, msg, (int)strlen(msg), 0);

    while (total < taille - 1 && (n = recv(s, rep + total, taille - 1 - total, 0)) > 0)
        total += n;
    rep[total] = '\0';
    while (total > 0 && (rep[total-1] == '\n' || rep[total-1] == '\r'))
        rep[--total] = '\0';

    closesocket(s);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Petits outils d'interface                                          */
/* ------------------------------------------------------------------ */
static HWND ctl(HWND parent, const char *classe, const char *texte, DWORD style,
                int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(0, classe, texte, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, parent, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessage(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return c;
}

static void combo_texte(HWND combo, char *dest, int taille)
{
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    dest[0] = '\0';
    if (sel != CB_ERR) SendMessageA(combo, CB_GETLBTEXT, sel, (LPARAM)dest);
    (void)taille;
}

/* Remplit un combo avec "OK a;b;c" */
static void remplir_combo(HWND combo, const char *rep)
{
    char copie[2048], *tok;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    if (strncmp(rep, "OK ", 3) != 0) return;
    strncpy(copie, rep + 3, sizeof(copie) - 1);
    copie[sizeof(copie) - 1] = '\0';
    for (tok = strtok(copie, ";"); tok; tok = strtok(NULL, ";"))
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)tok);
    SendMessage(combo, CB_SETCURSEL, 0, 0);
}

static void statut(const char *txt) { SetWindowTextA(g_statut, txt); }

static void maj_bouton_toggle(void)
{
    int sel = (int)SendMessage(g_liste, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || sel >= g_nb) {
        SetWindowTextA(g_btnToggle, "Allumer");
        EnableWindow(g_btnToggle, FALSE);
        return;
    }
    EnableWindow(g_btnToggle, TRUE);
    SetWindowTextA(g_btnToggle, g_etats[sel] == 1 ? "Eteindre" : "Allumer");
}

/* ------------------------------------------------------------------ */
/*  Chargement des donnees depuis le serveur                           */
/* ------------------------------------------------------------------ */
static void charger_equipements(int rafraichir)
{
    char etage[64], piece[64], req[200], rep[4096], copie[4096], ligne[160], *tok;
    int selAvant = (int)SendMessage(g_liste, LB_GETCURSEL, 0, 0);

    SendMessage(g_liste, LB_RESETCONTENT, 0, 0);
    g_nb = 0;
    combo_texte(g_cbEtage, etage, sizeof(etage));
    combo_texte(g_cbPiece, piece, sizeof(piece));
    if (!etage[0] || !piece[0]) { maj_bouton_toggle(); return; }

    snprintf(req, sizeof(req), "%s %s %s", rafraichir ? "RAFRAICHIR" : "EQUIPEMENTS", etage, piece);
    if (requete(req, rep, sizeof(rep)) != 0) { statut("Serveur injoignable (port 5000)."); return; }
    if (strncmp(rep, "OK", 2) != 0) { statut(rep); return; }

    strncpy(copie, rep + 2, sizeof(copie) - 1);
    copie[sizeof(copie) - 1] = '\0';
    for (tok = strtok(copie, "; "); tok && g_nb < MAX_EQ; tok = strtok(NULL, ";")) {
        int id, etat;
        char nom[64];
        if (sscanf(tok, "%d,%63[^,],%d", &id, nom, &etat) == 3) {
            const char *txt = (etat == 1) ? "ALLUME " : (etat == 0 ? "ETEINT " : "INCONNU");
            snprintf(ligne, sizeof(ligne), "[%s]  %s", txt, nom);
            SendMessageA(g_liste, LB_ADDSTRING, 0, (LPARAM)ligne);
            g_ids[g_nb] = id;
            g_etats[g_nb] = etat;
            g_nb++;
        }
    }
    if (selAvant != LB_ERR && selAvant < g_nb)
        SendMessage(g_liste, LB_SETCURSEL, selAvant, 0);
    maj_bouton_toggle();
    statut(rafraichir ? "Etats mis a jour depuis SimDom." : "Pret.");
}

static void charger_pieces(void)
{
    char etage[64], req[100], rep[2048];
    combo_texte(g_cbEtage, etage, sizeof(etage));
    snprintf(req, sizeof(req), "PIECES %s", etage);
    if (requete(req, rep, sizeof(rep)) == 0) remplir_combo(g_cbPiece, rep);
    charger_equipements(0);
}

static void charger_etages(void)
{
    char rep[2048];
    if (requete("ETAGES", rep, sizeof(rep)) != 0) {
        statut("Serveur injoignable (port 5000).");
        return;
    }
    remplir_combo(g_cbEtage, rep);
    charger_pieces();
}

static void basculer(void)
{
    int sel = (int)SendMessage(g_liste, LB_GETCURSEL, 0, 0);
    char req[64], rep[256];
    if (sel == LB_ERR || sel >= g_nb) return;

    snprintf(req, sizeof(req), "%s %d", g_etats[sel] == 1 ? "ETEINDRE" : "ALLUMER", g_ids[sel]);
    if (requete(req, rep, sizeof(rep)) != 0) { statut("Serveur injoignable."); return; }
    if (strncmp(rep, "OK", 2) != 0) { statut(rep); return; }
    charger_equipements(0);
}

/* ------------------------------------------------------------------ */
/*  Fenetre "Commande des equipements" (pilotage direct)               */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK ProcPilotage(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        ctl(h, "STATIC", "Etage(s) :", 0, 15, 18, 120, 20, 0);
        g_cbEtage = ctl(h, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 150, 15, 250, 200, ID_COMBO_ETAGE);
        ctl(h, "STATIC", "Piece(s) / emplacement(s) :", 0, 15, 53, 135, 20, 0);
        g_cbPiece = ctl(h, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 150, 50, 250, 200, ID_COMBO_PIECE);
        ctl(h, "BUTTON", "Equipements de la piece", BS_GROUPBOX, 10, 85, 400, 190, 0);
        g_liste = ctl(h, "LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 20, 105, 380, 160, ID_LISTE);
        g_btnRaf    = ctl(h, "BUTTON", "Rafraichir", BS_PUSHBUTTON, 20, 290, 100, 28, ID_BTN_RAFRAICHIR);
        g_btnToggle = ctl(h, "BUTTON", "Allumer",    BS_PUSHBUTTON, 150, 290, 110, 28, ID_BTN_TOGGLE);
        ctl(h, "BUTTON", "Fermer", BS_PUSHBUTTON, 300, 290, 100, 28, ID_BTN_FERMER);
        g_statut = ctl(h, "STATIC", "", 0, 15, 330, 390, 20, 0);
        EnableWindow(g_btnToggle, FALSE);
        charger_etages();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_COMBO_ETAGE:
            if (HIWORD(w) == CBN_SELCHANGE) charger_pieces();
            break;
        case ID_COMBO_PIECE:
            if (HIWORD(w) == CBN_SELCHANGE) charger_equipements(0);
            break;
        case ID_LISTE:
            if (HIWORD(w) == LBN_SELCHANGE) maj_bouton_toggle();
            break;
        case ID_BTN_RAFRAICHIR: charger_equipements(1); break;
        case ID_BTN_TOGGLE:     basculer(); break;
        case ID_BTN_FERMER:     DestroyWindow(h); break;
        }
        return 0;

    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

    case WM_DESTROY:
        g_pilotage = NULL;
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/*  Menu principal                                                     */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK ProcMenu(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        ctl(h, "BUTTON", "Connexion", BS_PUSHBUTTON, 10, 10, 110, 50, ID_BTN_CONNEXION);
        g_btnPilotage = ctl(h, "BUTTON", "Pilotage direct", BS_PUSHBUTTON, 130, 10, 110, 50, ID_BTN_PILOTAGE);
        ctl(h, "BUTTON", "A propos", BS_PUSHBUTTON, 250, 10, 110, 50, ID_BTN_APROPOS);
        EnableWindow(g_btnPilotage, FALSE);   /* grise tant qu'on n'est pas connecte */
        return 0;

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_BTN_CONNEXION: {
            char rep[64];
            if (requete("PING", rep, sizeof(rep)) == 0 && strncmp(rep, "OK", 2) == 0) {
                EnableWindow(g_btnPilotage, TRUE);
                MessageBoxA(h, "Connecte au serveur domotique.", "Connexion", MB_OK | MB_ICONINFORMATION);
            } else {
                EnableWindow(g_btnPilotage, FALSE);
                MessageBoxA(h, "Serveur injoignable.\nLance d'abord Serveur.exe.", "Connexion", MB_OK | MB_ICONERROR);
            }
            break;
        }
        case ID_BTN_PILOTAGE:
            if (g_pilotage) { SetForegroundWindow(g_pilotage); break; }
            g_pilotage = CreateWindowExA(0, "FenetrePilotage", "Commande des equipements",
                                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 435, 400,
                                         h, NULL, g_inst, NULL);
            ShowWindow(g_pilotage, SW_SHOW);
            break;
        case ID_BTN_APROPOS:
            MessageBoxA(h, "Client domotique V0.1\nProjet ESGI 2026", "A propos", MB_OK | MB_ICONINFORMATION);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    WSADATA wsa;
    MSG msg;
    (void)prev; (void)cmd;

    g_inst = inst;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    memset(&wc, 0, sizeof(wc));
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    wc.lpfnWndProc = ProcMenu;
    wc.lpszClassName = "FenetreMenu";
    RegisterClassA(&wc);

    wc.lpfnWndProc = ProcPilotage;
    wc.lpszClassName = "FenetrePilotage";
    RegisterClassA(&wc);

    g_menu = CreateWindowExA(0, "FenetreMenu", "Client domotique V0.1",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 385, 110,
                             NULL, NULL, inst, NULL);
    ShowWindow(g_menu, show);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    WSACleanup();
    return 0;
}