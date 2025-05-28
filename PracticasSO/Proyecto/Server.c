#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <openssl/sha.h>
#include <ncurses.h>

#define MAX_CLIENTES 10
#define SHM_SIZE 256
#define ARCHIVO_USUARIOS "usuarios.txt"

typedef struct {
    int client_num;
    char shm_name[64];
    char sem_req_name[64];
    char sem_resp_name[64];
} ClienteInfo;

void hash_sha256(const char *input, char *output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output + (i * 2), "%02x", hash[i]);
    output[64] = 0;
}

void print_center(WINDOW *win, int y, int width, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    int x = (width - strlen(buffer)) / 2;
    mvwprintw(win, y, x, "%s", buffer);
    wrefresh(win);
}

int usuario_valido(const char *user, const char *pass_hash) {
    FILE *file = fopen(ARCHIVO_USUARIOS, "r");
    if (!file) return 0;
    char u[64], p[65];
    while (fscanf(file, "%s %s", u, p) == 2) {
        if (strcmp(user, u) == 0 && strcmp(pass_hash, p) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

void registrar_usuario(const char *user, const char *pass_hash) {
    FILE *file = fopen(ARCHIVO_USUARIOS, "a");
    if (file) {
        fprintf(file, "%s %s\n", user, pass_hash);
        fclose(file);
    }
}

void *cliente_thread(void *arg) {
    ClienteInfo *client = (ClienteInfo *)arg;

    int shm_fd = shm_open(client->shm_name, O_RDWR, 0666);
    char *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    sem_t *sem_req = sem_open(client->sem_req_name, 0);
    sem_t *sem_resp = sem_open(client->sem_resp_name, 0);

    char usuario[64], password[64], hash[65], respuesta[64];
    while (1) {
        sem_wait(sem_req);

        sscanf(shm_ptr, "%s %s", usuario, password);
        hash_sha256(password, hash);

        if (strcmp(usuario, "REGISTRAR") == 0) {
            sscanf(password, "%s", password);
            hash_sha256(password, hash);
            registrar_usuario(password, hash);
            snprintf(respuesta, sizeof(respuesta), "Registrado correctamente.");
        } else if (usuario_valido(usuario, hash)) {
            snprintf(respuesta, sizeof(respuesta), "Inicio de sesión exitoso.");
        } else {
            snprintf(respuesta, sizeof(respuesta), "Credenciales inválidas.");
        }

        strncpy(shm_ptr, respuesta, SHM_SIZE);
        sem_post(sem_resp);
    }
    return NULL;
}

int main() {
    initscr(); noecho(); cbreak();
    int width = COLS;

    ClienteInfo clientes[MAX_CLIENTES];
    pthread_t hilos[MAX_CLIENTES];

    for (int i = 0; i < MAX_CLIENTES; i++) {
        snprintf(clientes[i].shm_name, 64, "/shm_cliente_%d", i);
        snprintf(clientes[i].sem_req_name, 64, "/sem_req_%d", i);
        snprintf(clientes[i].sem_resp_name, 64, "/sem_resp_%d", i);
        clientes[i].client_num = i;

        shm_unlink(clientes[i].shm_name);
        int shm_fd = shm_open(clientes[i].shm_name, O_CREAT | O_RDWR, 0666);
        ftruncate(shm_fd, SHM_SIZE);
        close(shm_fd);

        sem_unlink(clientes[i].sem_req_name);
        sem_unlink(clientes[i].sem_resp_name);
        sem_open(clientes[i].sem_req_name, O_CREAT, 0666, 0);
        sem_open(clientes[i].sem_resp_name, O_CREAT, 0666, 0);

        pthread_create(&hilos[i], NULL, cliente_thread, &clientes[i]);
        print_center(stdscr, 2+i, width, "Cliente %d esperando conexión...", i);
    }

    print_center(stdscr, 1, width, "[Servidor] Activo. Esperando clientes...");
    getch();
    endwin();
    return 0;
}
