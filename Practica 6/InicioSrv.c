#include <ncurses.h>
#include <string.h>

void get_hidden_input(WINDOW *win, int y, int x, char *input, int max_len) {
    int ch, i = 0;
    wmove(win, y, x);
    wrefresh(win);
    while ((ch = wgetch(win)) != '\n' && i < max_len - 1) {
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

void limpiar_linea(WINDOW *win, int y, int x, int ancho) {
    mvwprintw(win, y, x, "%-*s", ancho, "");
    wrefresh(win);
}



int main() {
    char username[30];
    char password[30];

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int height = 15, width = 60;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, starty, startx);
    box(win, 0, 0);
    mvwprintw(win, 1, (width - 18) / 2, "Inicio de Sesión");
    mvwprintw(win, 3, 2, "Usuario: (Escriba 'registrarse' para crear cuenta)");
    mvwprintw(win, 5, 2, "Usuario:");
    mvwprintw(win, 7, 2, "Contraseña:");
    wrefresh(win);

    int intentos = 0;
    int success = 0;

    while (!success && intentos < 3) {
        limpiar_linea(win, 5, 12, width - 14);
        limpiar_linea(win, 7, 14, width - 16);
        limpiar_linea(win, 9, 2, width - 4);
        wrefresh(win);

        echo();
        mvwgetnstr(win, 5, 12, username, sizeof(username) - 1);
        noecho();

        if (strcmp(username, "registrarse") == 0) {
            limpiar_linea(win, 9, 2, width - 4);
            mvwprintw(win, 9, 2, "Simulación de registro completada. Ahora inicie sesión.");
            wrefresh(win);
            napms(1500);
            continue;
        }

        get_hidden_input(win, 7, 14, password, sizeof(password));

        if (strcmp(username, "admin") == 0 && strcmp(password, "1234") == 0) {
            mvwprintw(win, 9, 2, "¡Acceso concedido!");
            success = 1;
        } else {
            intentos++;
            mvwprintw(win, 9, 2, "Usuario o contraseña incorrectos. Intento %d de 3.", intentos);
        }

        wrefresh(win);
        if (!success) napms(1200);
    }

    if (!success) {
        limpiar_linea(win, 9, 2, width - 4);
        mvwprintw(win, 9, 2, "Demasiados intentos fallidos. Cerrando sesión.");
        wrefresh(win);
        napms(1500);
        delwin(win);
        endwin();  // Cierre limpio de ncurses
        return 1;  // Finalización con error
    }

    wgetch(win);  // Solo si tuvo éxito
    delwin(win);
    endwin();
    return 0;
}
