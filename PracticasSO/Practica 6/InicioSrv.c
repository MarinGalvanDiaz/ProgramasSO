#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <string.h>

void handleErrors(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
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


void get_hidden_input(WINDOW *win, int y, int x, char *input, int max_len)
{
    int ch, i = 0;
    wmove(win, y, x);
    wrefresh(win);
    while ((ch = wgetch(win)) != '\n' && i < max_len - 1)
    {
        if (ch == KEY_BACKSPACE || ch == 127)
        {
            if (i > 0)
            {
                i--;
                int cur_y, cur_x;
                getyx(win, cur_y, cur_x);
                mvwaddch(win, cur_y, cur_x - 1, ' ');
                wmove(win, cur_y, cur_x - 1);
                wrefresh(win);
            }
        }
        else if (ch >= 32 && ch <= 126)
        {
            input[i++] = ch;
            waddch(win, '*');
            wrefresh(win);
        }
    }
    input[i] = '\0';
}

void limpiar_linea(WINDOW *win, int y, int x, int ancho)
{
    mvwprintw(win, y, x, "%-*s", ancho, "");
    wrefresh(win);
}

void pantalla_login(WINDOW *win)
{
    char username[30], password[30];
    int intentos = 0, success = 0;
    int width = 60;

    while (!success && intentos < 3)
    {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, (width - 18) / 2, "Inicio de Sesión");
        mvwprintw(win, 3, 2, "Usuario:");
        mvwprintw(win, 5, 2, "Contraseña:");
        wrefresh(win);

        echo();
        mvwgetnstr(win, 3, 12, username, sizeof(username) - 1);
        noecho();
        get_hidden_input(win, 5, 14, password, sizeof(password));


        if (strcmp(username, "admin") == 0 && strcmp(password, "1234") == 0)
        {
            mvwprintw(win, 7, 2, "¡Acceso concedido!");
            wrefresh(win);
            success = 1;
        }
        else
        {
            intentos++;
            mvwprintw(win, 7, 2, "Error. Intento %d de 3.", intentos);
            wrefresh(win);
            napms(1000);
        }
    }

    if (!success)
    {
        mvwprintw(win, 9, 2, "Demasiados intentos. Cerrando...");
        wrefresh(win);
        napms(1500);
        delwin(win);
        endwin();
        exit(1);
    }

    mvwprintw(win, 9, 2, "Presione una tecla para continuar...");
    werase(win);
    box(win, 0, 0);
    wrefresh(win);
    flushinp();
}

void pantalla_registro(WINDOW *win)
{
    char usuario[30], pass1[30], pass2[30];
    int width = 60;

    while (1)
    {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, (width - 17) / 2, "Registro de Usuario");

        mvwprintw(win, 3, 2, "Nuevo usuario:");
        mvwprintw(win, 5, 2, "Contraseña:");
        mvwprintw(win, 7, 2, "Repetir contraseña:");

        // Limpiar entradas anteriores
        mvwprintw(win, 3, 18, "%-30s", " ");
        mvwprintw(win, 5, 14, "%-30s", " ");
        mvwprintw(win, 7, 22, "%-30s", " ");
        wrefresh(win);

        echo();
        mvwgetnstr(win, 3, 18, usuario, sizeof(usuario) - 1);
        noecho();
        get_hidden_input(win, 5, 14, pass1, sizeof(pass1));
        get_hidden_input(win, 7, 22, pass2, sizeof(pass2));

        if (strlen(usuario) == 0 || strlen(pass1) == 0 || strlen(pass2) == 0)
        {
            mvwprintw(win, 9, 2, "Campos vacíos no permitidos.");
            wclrtoeol(win);
        }
        else if (strcmp(pass1, pass2) != 0)
        {
            mvwprintw(win, 9, 2, "Las contraseñas no coinciden.");
            wclrtoeol(win);
        }
        else
        {
            mvwprintw(win, 9, 2, "Registro exitoso.");
            
            wrefresh(win);
            napms(1000);
            break;
        }

        

        wrefresh(win);
        napms(1500);
    }

    mvwprintw(win, 11, 2, "Presione una tecla para continuar...");
    werase(win);
    box(win, 0, 0);
    wrefresh(win);
    flushinp();
}

void menu_principal(WINDOW *win)
{
    int opcion = 0;
    keypad(win, TRUE);

    while (1)
    {
        werase(win);    // Limpia todo
        box(win, 0, 0); // Redibuja el borde
        mvwprintw(win, 1, 2, "Bienvenido - Seleccione una opción:");

        mvwprintw(win, 3, 4, opcion == 0 ? "> Iniciar Sesión" : "  Iniciar Sesión");
        mvwprintw(win, 4, 4, opcion == 1 ? "> Registrarse" : "  Registrarse");
        mvwprintw(win, 6, 4, "Use las flechas y ENTER para seleccionar");

        wrefresh(win);
        flushinp(); // Limpia entrada pendiente

        int ch = wgetch(win);
        switch (ch)
        {
        case KEY_UP:
            opcion = (opcion == 0) ? 1 : 0;
            break;
        case KEY_DOWN:
            opcion = (opcion == 1) ? 0 : 1;
            break;
        case '\n':
            werase(win); // Limpia ventana antes de ir a la otra pantalla
            wrefresh(win);
            flushinp();
            if (opcion == 0)
                pantalla_login(win);
            else
                pantalla_registro(win);
            break;
        }
    }
}

int main()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int height = 15, width = 60;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, starty, startx);
    menu_principal(win);

    delwin(win);
    endwin();
    unsigned char key[32] = "0123456789abcdef0123456789abcdef";
    unsigned char iv[16] = "abcdef9876543210";

    unsigned char plaintext[] = "Mensaje super secreto que cifrar";
    unsigned char ciphertext[128];
    unsigned char decryptedtext[128];

    int ciphertext_len = aes_encrypt(plaintext, strlen((char *)plaintext), key, iv, ciphertext);

    printf("Texto cifrado (hex): ");
    for (int i = 0; i < ciphertext_len; i++)
        printf("%02x", ciphertext[i]);
    printf("\n");

    int decryptedtext_len = aes_decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext);
    decryptedtext[decryptedtext_len] = '\0'; // null terminate

    printf("Texto descifrado: %s\n", decryptedtext);
    return 0;
}
