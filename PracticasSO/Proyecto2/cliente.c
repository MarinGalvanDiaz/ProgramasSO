#include "common.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <openssl/evp.h>
#include <ncurses.h>
#include <string.h>

unsigned char aes_key[32]; // Misma clave que el servidor

// Prototipos
int initial_menu();
int register_screen();
int check_server_connection();
void get_password(WINDOW *win, int y, int x, char *buffer, int max_len);
int login_screen(char *out_username);
void app_menu(const char *username);

// Comprueba que el servidor esté disponible
int check_server_connection()
{
    key_t key = ftok(KEY_FILE, FTOK_ID);
    if (key == -1)
        return 0;
    int shmid = shmget(key, sizeof(SharedData), 0666);
    if (shmid == -1)
        return 0;
    sem_t *sem_server = sem_open(SEM_SERVER, 0);
    sem_t *sem_client = sem_open(SEM_CLIENT, 0);
    if (sem_server == SEM_FAILED || sem_client == SEM_FAILED)
        return 0;
    sem_close(sem_server);
    sem_close(sem_client);
    return 1;
}

void show_connection_error() {
    int height = 7, width = 40;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, starty, startx);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(2));  // Mismo esquema que login

    wattron(win, A_BOLD);
    mvwprintw(win, 2, (width - 23) / 2, "Error de conexi\303\263n");
    wattroff(win, A_BOLD);

    mvwprintw(win, 4, 2, "No se puede conectar con el servidor.");
    mvwprintw(win, 5, 2, "Presione cualquier tecla para continuar...");
    wrefresh(win);

    wgetch(win);
    delwin(win);
}


int initial_menu()
{
    int choice = 0;
    const char *options[] = {
        "Iniciar sesi\303\263n",
        "Registrarse",
        "Salir"};
    int n_options = sizeof(options) / sizeof(options[0]);

    WINDOW *menu = newwin(10, 40, (LINES - 10) / 2, (COLS - 40) / 2);
    box(menu, 0, 0);
    keypad(menu, TRUE);
    wbkgd(menu, COLOR_PAIR(2));

    while (1)
    {
        wclear(stdscr);
        refresh();
        werase(menu);
        box(menu, 0, 0);
        mvwprintw(menu, 1, 2, " Seleccione una opci\303\263n ");

        for (int i = 0; i < n_options; i++)
        {
            if (i == choice)
            {
                wattron(menu, A_REVERSE);
                mvwprintw(menu, 3 + i * 2, 4, "%s", options[i]);
                wattroff(menu, A_REVERSE);
            }
            else
            {
                mvwprintw(menu, 3 + i * 2, 4, "%s", options[i]);
            }
        }

        wrefresh(menu);
        int ch = wgetch(menu);
        if (ch == KEY_UP)
            choice = (choice == 0) ? n_options - 1 : choice - 1;
        else if (ch == KEY_DOWN)
            choice = (choice == n_options - 1) ? 0 : choice + 1;
        else if (ch == '\n')
        {
            delwin(menu);
            return choice;
        }
    }
}

// NUEVO: pantalla de registro
int register_screen()
{
    wclear(stdscr);
    refresh();
    int height = 10, width = 40;
    int starty = (LINES - height) / 2, startx = (COLS - width) / 2;
    WINDOW *win = newwin(height, width, starty, startx);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(2));
    mvwprintw(win, 1, (width - 11) / 2, " REGISTRO ");
    mvwprintw(win, 3, 2, "Usuario: ");
    mvwprintw(win, 5, 2, "Contrase\303\261a: ");
    wrefresh(win);

    char username[MAX_USER_LEN], password[MAX_PASS_LEN];
    echo();
    get_username(win, 3, 12, username, MAX_USER_LEN);
    noecho();
    get_password(win, 5, 14, password, MAX_PASS_LEN - 1);

    if (!check_server_connection()) {
    mvwprintw(win, 7, 2, "No se puede conectar al servidor");
    mvwprintw(win, 8, 2, "Presione cualquier tecla...");
    wrefresh(win);
    wgetch(win);
    delwin(win);
    return 0;
}

    unsigned char enc_u[128], enc_p[128];
    int len_u = encrypt_aes256((unsigned char *)username, strlen(username), enc_u, aes_key);
    int len_p = encrypt_aes256((unsigned char *)password, strlen(password), enc_p, aes_key);

    key_t key = ftok(KEY_FILE, FTOK_ID);
    int shmid = shmget(key, sizeof(SharedData), 0666);
    SharedData *sd = (SharedData *)shmat(shmid, NULL, 0);

    sd->action = 'R';
    memcpy(sd->username, enc_u, len_u);
    sd->username_len = len_u;
    memcpy(sd->password, enc_p, len_p);
    sd->password_len = len_p;
    sd->client_pid = getpid();

    sem_t *s_s = sem_open(SEM_SERVER, 0);
    sem_t *s_c = sem_open(SEM_CLIENT, 0);
    sem_post(s_s);
    sem_wait(s_c);

    int ok = (sd->authenticated == 1);
    if (ok)
    {
        mvwprintw(win, 7, 2, "Registro exitoso.");
    }
    else
    {
        mvwprintw(win, 7, 2, "Error: usuario ya existe.");
    }
    mvwprintw(win, 8, 2, "Presione una tecla...");
    wrefresh(win);
    wgetch(win);

    // Limpieza y regreso
    shmdt(sd);
    sem_close(s_s);
    sem_close(s_c);
    delwin(win);

    return 1; // siempre regresar al menú principal
}
// Lee contraseña ocultando con '*'
void get_password(WINDOW *win, int y, int x, char *buffer, int max_len)
{
    int pos = 0, ch;
    keypad(win, TRUE);
    curs_set(1);
    noecho();
    wmove(win, y, x);

    while (1)
    {
        ch = wgetch(win);
        if (ch == '\n' || ch == '\r')
            break;

        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && pos > 0)
        {
            pos--;
            buffer[pos] = '\0';
            mvwaddch(win, y, x + pos, ' ');
            wmove(win, y, x + pos);
        }
        else if (isprint(ch) && pos < max_len - 1)
        {
            buffer[pos++] = ch;
            mvwaddch(win, y, x + pos - 1, '*');
        }
        wrefresh(win);
    }
    buffer[pos] = '\0';
    curs_set(0);
    echo();
}

// Pantalla de login. Devuelve 1 si hay éxito y almacena el usuario en out_username
int login_screen(char *out_username)
{
    wclear(stdscr);
    refresh();
    int height = 10, width = 40;
    int starty = (LINES - height) / 2, startx = (COLS - width) / 2;
    WINDOW *win = newwin(height, width, starty, startx);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(2));
    mvwprintw(win, 1, (width - 12) / 2, " INICIAR SESI\303\213N ");
    mvwprintw(win, 3, 2, "Usuario: ");
    mvwprintw(win, 5, 2, "Contraseña: ");
    wrefresh(win);

    // Leer credenciales
    char username[MAX_USER_LEN], password[MAX_PASS_LEN];
    echo();
    get_username(win, 3, 12, username, MAX_USER_LEN);
    noecho();
    get_password(win, 5, 14, password, MAX_PASS_LEN - 1);

    // Conexión servidor
    if (!check_server_connection())
    {
        mvwprintw(win, 7, 2, "No se puede conectar al servidor");
        mvwprintw(win, 8, 2, "Presione cualquier tecla...");
        wrefresh(win);
        wgetch(win);
        delwin(win);
        return 0;
    }

    // Cifrar
    unsigned char enc_u[128], enc_p[128];
    int len_u = encrypt_aes256((unsigned char *)username, strlen(username), enc_u, aes_key);
    int len_p = encrypt_aes256((unsigned char *)password, strlen(password), enc_p, aes_key);

    // Compartir
    key_t key = ftok(KEY_FILE, FTOK_ID);
    int shmid = shmget(key, sizeof(SharedData), 0666);
    SharedData *sd = (SharedData *)shmat(shmid, NULL, 0);

    sd->action = 'L';
    memcpy(sd->username, enc_u, len_u);
    sd->username_len = len_u;
    memcpy(sd->password, enc_p, len_p);
    sd->password_len = len_p;
    sd->client_pid = getpid();

    sem_t *s_s = sem_open(SEM_SERVER, 0);
    sem_t *s_c = sem_open(SEM_CLIENT, 0);
    sem_post(s_s);
    sem_wait(s_c);

    // Leer respuesta
    int ok = (sd->authenticated == 1);
    if (ok)
    {
        // Guardamos el usuario para el menú
        strcpy(out_username, username);
    }
    else
    {
        mvwprintw(win, 7, 2, "Usuario o contraseña incorrectos");
        mvwprintw(win, 8, 2, "Presione cualquier tecla...");
        wrefresh(win);
        wgetch(win);
    }

    // Limpieza
    shmdt(sd);
    sem_close(s_s);
    sem_close(s_c);
    delwin(win);
    return ok;
}

// Menú principal de la aplicación
void app_menu(const char *username)
{
    int choice = 0;
    const char *options[] = {
        "Ver catálogo",
        "Ver carrito de compra",
        "Realizar compra",
        "Ver perfil de usuario",
        "Salir"};
    int n_options = sizeof(options) / sizeof(options[0]);

    WINDOW *menu = newwin(LINES - 4, COLS - 4, 2, 2);
    keypad(menu, TRUE);

    while (1)
    {
        wclear(stdscr);
        refresh();
        werase(menu);
        box(menu, 0, 0);
        wattron(menu, COLOR_PAIR(3) | A_BOLD);
        mvwprintw(menu, 1, 2, " ¡Bienvenido, %s! ", username);
        wattroff(menu, COLOR_PAIR(3) | A_BOLD);

        for (int i = 0; i < n_options; i++)
        {
            if (i == choice)
            {
                wattron(menu, A_REVERSE);
                mvwprintw(menu, 3 + i * 2, 4, "%s", options[i]);
                wattroff(menu, A_REVERSE);
            }
            else
            {
                mvwprintw(menu, 3 + i * 2, 4, "%s", options[i]);
            }
        }
        wrefresh(menu);

        int ch = wgetch(menu);
        if (ch == KEY_UP)
            choice = (choice == 0) ? n_options - 1 : choice - 1;
        else if (ch == KEY_DOWN)
            choice = (choice == n_options - 1) ? 0 : choice + 1;
        else if (ch == '\n')
        {
            if (choice == n_options - 1)
                break; // Salir
            // Aquí podrías llamar a funciones stubs, p.ej.:
            // if (choice==0) view_catalog();
            // ...
            // Por ahora:
            mvwprintw(menu, LINES - 6, 4, "Has elegido: %s", options[choice]);
            mvwprintw(menu, LINES - 5, 4, "Presiona una tecla para volver...");
            wrefresh(menu);
            wgetch(menu);
        }
    }

    delwin(menu);
    wclear(menu);
    wrefresh(menu);
}

int main() {
    // Validar conexión antes de mostrar login o registro
    if (!check_server_connection()) {
        show_connection_error();
        endwin();
        return 1;  // Termina programa por falta de conexión
    }
    initscr();
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    }
    cbreak(); noecho(); curs_set(0);

    memset(aes_key, 0x2A, sizeof(aes_key));
    srand(time(NULL));

    

    char username[MAX_USER_LEN];
    while (1) {
        int choice = initial_menu(); // Puedes crear un menú inicial para Login / Registro / Salir
        if (choice == 0) {
            if (login_screen(username)) {
                app_menu(username);
            }
        } else if (choice == 1) {
            register_screen();
        } else {
            break;  // Salir
        }
    }

    endwin();
    return 0;
}
