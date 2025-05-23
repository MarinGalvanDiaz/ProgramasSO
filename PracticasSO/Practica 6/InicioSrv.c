#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <time.h>

#define MAX_LINEA 1024

char usuario_autenticado[30]; // Usuario que inició sesión

void handleErrors(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

int validar_contrasena_segura(const char *pass) {
    if (strlen(pass) < 8) return 0;
    int tiene_letra = 0, tiene_numero = 0, tiene_simbolo = 0;
    for (int i = 0; pass[i]; i++) {
        if ((pass[i] >= 'A' && pass[i] <= 'Z') || (pass[i] >= 'a' && pass[i] <= 'z')) tiene_letra = 1;
        else if (pass[i] >= '0' && pass[i] <= '9') tiene_numero = 1;
        else tiene_simbolo = 1;
    }
    return tiene_letra && tiene_numero && tiene_simbolo;
}

int aes_encrypt(unsigned char *plaintext, int plaintext_len,
                unsigned char *key, unsigned char *iv,
                unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx;
    int len, ciphertext_len;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handleErrors("Error creando contexto cifrado.");

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors("Error inicializando cifrado.");

    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        handleErrors("Error cifrando datos.");
    ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        handleErrors("Error finalizando cifrado.");
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int aes_decrypt(unsigned char *ciphertext, int ciphertext_len,
                unsigned char *key, unsigned char *iv,
                unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len, plaintext_len;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) handleErrors("Error creando contexto descifrado.");

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors("Error inicializando descifrado.");

    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors("Error descifrando datos.");
    plaintext_len = len;

    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
        handleErrors("Error finalizando descifrado.");
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

void guardar_usuario_cifrado(const char *usuario, const char *password,
                             unsigned char *key, unsigned char *iv) {
    char texto[256];
    snprintf(texto, sizeof(texto), "%s,%s", usuario, password);

    unsigned char ciphertext[512];
    int cipher_len = aes_encrypt((unsigned char *)texto, strlen(texto), key, iv, ciphertext);

    FILE *f = fopen("usuarios_cifrado.txt", "a");
    if (!f) {
        perror("No se pudo abrir archivo para guardar");
        return;
    }

    for (int i = 0; i < cipher_len; i++)
        fprintf(f, "%02x", ciphertext[i]);
    fprintf(f, "\n");
    fclose(f);
}

void escribir_log_cifrado(const char *usuario, unsigned char *key, unsigned char *iv) {
    time_t ahora = time(NULL);
    struct tm *tm_info = localtime(&ahora);
    char fecha_hora[64];
    strftime(fecha_hora, sizeof(fecha_hora), "%Y-%m-%d %H:%M:%S", tm_info);

    char log_entry[128];
    snprintf(log_entry, sizeof(log_entry), "Usuario: %s - Fecha: %s", usuario, fecha_hora);

    unsigned char ciphertext[256];
    int len = aes_encrypt((unsigned char*)log_entry, strlen(log_entry), key, iv, ciphertext);

    FILE *f = fopen("log_cifrado.txt", "a");
    if (!f) {
        perror("Error al abrir archivo de log");
        return;
    }

    for (int i = 0; i < len; i++)
        fprintf(f, "%02x", ciphertext[i]);
    fprintf(f, "\n");
    fclose(f);
}

int validar_login(const char *usuario, const char *password,
                  unsigned char *key, unsigned char *iv) {
    FILE *f = fopen("usuarios_cifrado.txt", "r");
    if (!f) return 0;

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), f)) {
        int len = strlen(linea);
        if (linea[len - 1] == '\n') linea[len - 1] = '\0';

        int cipher_len = strlen(linea) / 2;
        unsigned char ciphertext[512];
        for (int i = 0; i < cipher_len; i++) {
            sscanf(linea + 2*i, "%2hhx", &ciphertext[i]);
        }

        unsigned char plaintext[512];
        int plain_len = aes_decrypt(ciphertext, cipher_len, key, iv, plaintext);
        plaintext[plain_len] = '\0';

        char *coma = strchr((char *)plaintext, ',');
        if (!coma) continue;

        *coma = '\0';
        char *u = (char *)plaintext;
        char *p = coma + 1;

        if (strcmp(u, usuario) == 0 && strcmp(p, password) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void get_hidden_input(WINDOW *win, int y, int x, char *input, int max_len) {
    int ch, i = 0;
    wmove(win, y, x);
    wrefresh(win);
    while ((ch = wgetch(win)) != '\n' && i < max_len - 1) {
        if (ch == 27) return;
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (i > 0) {
                i--;
                int cur_y, cur_x;
                getyx(win, cur_y, cur_x);
                mvwaddch(win, cur_y, cur_x - 1, ' ');
                wmove(win, cur_y, cur_x - 1);
                wrefresh(win);
            }
        } else if (ch >= 32 && ch <= 126) {
            input[i++] = ch;
            waddch(win, '*');
            wrefresh(win);
        }
    }
    input[i] = '\0';
}

void pantalla_bienvenida(WINDOW *win, const char *usuario) {
    wclear(stdscr);
    refresh();
    werase(win); box(win, 0, 0);
    mvwprintw(win, 2, 2, "Bienvenid@, %s", usuario);
    //mvwprintw(win, 4, 2, "La sesi\303\263n finalizar\303\241 en 10 segundos...");
    mvwprintw(win, 4, 2, "Este mensaje se autodestruir\303\241 en 10 segundos...");
    wrefresh(win);
    napms(10000);
    endwin();
    exit(0);
}

void mostrar_logs(WINDOW *win, unsigned char *key, unsigned char *iv) {
    FILE *f = fopen("log_cifrado.txt", "r");
    if (!f) {
        mvwprintw(win, 2, 2, "No se pudo abrir log_cifrado.txt");
        wrefresh(win);
        napms(2000);
        return;
    }

    wclear(stdscr);
    refresh();
    werase(win); box(win, 0, 0);
    mvwprintw(win, 1, 2, "Registros de inicio de sesi\303\263n:");

    char linea[MAX_LINEA];
    int y = 3;

    while (fgets(linea, sizeof(linea), f)) {
        int len = strlen(linea);
        if (linea[len - 1] == '\n') linea[len - 1] = '\0';

        int cipher_len = strlen(linea) / 2;
        unsigned char ciphertext[256];
        for (int i = 0; i < cipher_len; i++)
            sscanf(linea + 2*i, "%2hhx", &ciphertext[i]);

        unsigned char plaintext[256];
        int plain_len = aes_decrypt(ciphertext, cipher_len, key, iv, plaintext);
        plaintext[plain_len] = '\0';

        if (y < LINES - 2) {
            mvwprintw(win, y++, 2, "%s", plaintext);
        } else {
            mvwprintw(win, y, 2, "(Demasiados registros para mostrar)");
            break;
        }
    }

    fclose(f);
    mvwprintw(win, y + 2, 2, "Presione una tecla para volver...");
    wrefresh(win);
    wgetch(win);
}

void acceso_admin(WINDOW *win, unsigned char *key, unsigned char *iv) {
    char usuario[30], pass[30];

    wclear(stdscr);
    refresh();
    werase(win); box(win, 0, 0);
    mvwprintw(win, 1, 2, "Modo Administrador");
    mvwprintw(win, 3, 2, "Usuario: ");
    mvwprintw(win, 5, 2, "Clave: ");
    wrefresh(win);

    echo();
    mvwgetnstr(win, 3, 11, usuario, sizeof(usuario) - 1);
    noecho();
    get_hidden_input(win, 5, 9, pass, sizeof(pass));

    if (strcmp(usuario, "admin") == 0 && strcmp(pass, "admin123!") == 0) {
        mostrar_logs(win, key, iv);
    } else {
        mvwprintw(win, 7, 2, "Credenciales incorrectas.");
        wrefresh(win);
        napms(1500);
    }
}


void pantalla_login(WINDOW *win, unsigned char *key, unsigned char *iv) {
    char usuario[30], password[30];
    int intentos = 0, logueado = 0;

    while (!logueado && intentos < 3) {
        wclear(stdscr);
        refresh();
        werase(win); box(win, 0, 0);
        mvwprintw(win, 1, 2, "Inicio de Sesi\303\263n");
        mvwprintw(win, 3, 2, "Usuario: ");
        mvwprintw(win, 5, 2, "Contrase\303\261a: ");
        wrefresh(win);

        echo();
        mvwgetnstr(win, 3, 11, usuario, sizeof(usuario) - 1);
        noecho();

        get_hidden_input(win, 5, 14, password, sizeof(password));

        if (validar_login(usuario, password, key, iv)) {
            strcpy(usuario_autenticado, usuario);
            escribir_log_cifrado(usuario, key, iv);
            mvwprintw(win, 7, 2, "\302\241Acceso concedido!");
            wrefresh(win);
            napms(1500);
            pantalla_bienvenida(win, usuario_autenticado);
        } else {
            mvwprintw(win, 7, 2, "Usuario o contrase\303\261a incorrectos.");
        }
        intentos++;
        wrefresh(win);
        napms(1500);
    }

    if (!logueado) {
        mvwprintw(win, 9, 2, "Demasiados intentos. Saliendo...");
        wrefresh(win);
        napms(1500);
        endwin();
        exit(1);
    }
}

void pantalla_registro(WINDOW *win, unsigned char *key, unsigned char *iv) {
    char usuario[30], pass1[30], pass2[30];
    while (1) {
      
        wclear(stdscr);
        refresh();
        werase(win); box(win, 0, 0);
        mvwprintw(win, 1, 2, "Registro de Usuario");
        mvwprintw(win, 3, 2, "Nuevo usuario: ");
        mvwprintw(win, 5, 2, "Contrase\303\261a: ");
        mvwprintw(win, 7, 2, "Repetir contrase\303\261a: ");
        mvwprintw(win, 9, 2, ">=8 chars, 1 letra, 1 n\303\272mero, 1 s\303\255mbolo");

        echo();
        mvwgetnstr(win, 3, 18, usuario, sizeof(usuario) - 1);
        noecho();

        get_hidden_input(win, 5, 14, pass1, sizeof(pass1));
        get_hidden_input(win, 7, 22, pass2, sizeof(pass2));

        if (strcmp(pass1, pass2) != 0) {
            mvwprintw(win, 10, 2, "Las contrase\303\261as no coinciden.");
        } else if (!validar_contrasena_segura(pass1)) {
            mvwprintw(win, 10, 2, "Contrase\303\261a insegura.");
        } else if (validar_login(usuario, pass1, key, iv)) {
            mvwprintw(win, 10, 2, "El usuario ya existe.");
        } else {
            guardar_usuario_cifrado(usuario, pass1, key, iv);
            mvwprintw(win, 10, 2, "Registro exitoso.");
            wrefresh(win);
            napms(1500);
            break;
        }
        wrefresh(win);
        napms(1500);
    }
}

void menu_principal(WINDOW *win, unsigned char *key, unsigned char *iv) {
    int opcion = 0;
    const int total_opciones = 3;
    keypad(win, TRUE);
    while (1) {
        wclear(stdscr);
        refresh();
        werase(win); box(win, 0, 0);
        mvwprintw(win, 1, 2, "Seleccione una opci\303\263n:");
        mvwprintw(win, 3, 4, opcion == 0 ? "> Iniciar Sesi\303\263n" : "  Iniciar Sesi\303\263n");
        mvwprintw(win, 4, 4, opcion == 1 ? "> Registrarse" : "  Registrarse");
        mvwprintw(win, 5, 4, opcion == 2 ? "> Ver logs (admin)" : "  Ver logs (admin)");
        mvwprintw(win, 7, 4, "Usa flechas y ENTER");
        wrefresh(win);

        int ch = wgetch(win);
        switch (ch) {
            case KEY_UP: opcion = (opcion - 1 + total_opciones) % total_opciones; break;
            case KEY_DOWN: opcion = (opcion + 1) % total_opciones; break;
            case '\n':
                if (opcion == 0) pantalla_login(win, key, iv);
                else if (opcion == 1) pantalla_registro(win, key, iv);
                else if (opcion == 2) acceso_admin(win, key, iv);
                break;
        }
    }
}


int main() {
    unsigned char key[32] = "12345678901234567890123456789012";
    unsigned char iv[16]  = "1234567890123456";

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    menu_principal(stdscr, key, iv);

    endwin();
    return 0;
}
