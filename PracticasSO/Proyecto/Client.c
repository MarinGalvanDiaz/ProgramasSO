#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <openssl/sha.h>
#include <ncurses.h>

#define SHM_SIZE 256

void hash_sha256(const char *input, char *output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output + (i * 2), "%02x", hash[i]);
    output[64] = 0;
}

void leer_password(char *buffer, int max_len) {
    int i = 0;
    int ch;
    while ((ch = getch()) != '\n' && i < max_len - 1) {
        if (ch == 127 || ch == KEY_BACKSPACE) {
            if (i > 0) {
                i--;
                mvaddch(getcury(stdscr), getcurx(stdscr) - 1, ' ');
                move(getcury(stdscr), getcurx(stdscr) - 1);
            }
        } else {
            buffer[i++] = ch;
            addch('*');
        }
    }
    buffer[i] = '\0';
}

int main() {
    int client_id = getpid() % 10;

    char shm_name[64], sem_req_name[64], sem_resp_name[64];
    snprintf(shm_name, sizeof(shm_name), "/shm_cliente_%d", client_id);
    snprintf(sem_req_name, sizeof(sem_req_name), "/sem_req_%d", client_id);
    snprintf(sem_resp_name, sizeof(sem_resp_name), "/sem_resp_%d", client_id);

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    char *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    sem_t *sem_req = sem_open(sem_req_name, 0);
    sem_t *sem_resp = sem_open(sem_resp_name, 0);

    initscr(); noecho(); cbreak();

    char usuario[64], password[64];

    mvprintw(2, 2, "1. Iniciar sesión");
    mvprintw(3, 2, "2. Registrarse");
    mvprintw(4, 2, "Seleccione una opción: ");
    int opcion = getch() - '0';
    clear();

    echo();
    mvprintw(2, 2, opcion == 1 ? "Usuario: " : "Nuevo usuario: ");
    getnstr(usuario, sizeof(usuario));
    noecho();

    mvprintw(3, 2, "Contraseña: ");
    leer_password(password, sizeof(password));

    if (opcion == 2) strcpy(usuario, "REGISTRAR");

    snprintf(shm_ptr, SHM_SIZE, "%s %s", usuario, password);
    sem_post(sem_req);
    sem_wait(sem_resp);

    mvprintw(6, 2, "Respuesta del servidor: %s", shm_ptr);
    getch();
    endwin();

    return 0;
}
