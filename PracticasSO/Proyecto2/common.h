#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <ncurses.h>
#include <time.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <ctype.h>

#define SHM_SIZE 1024
#define MAX_USERS 100
#define MAX_CLIENTS 100
#define MAX_USER_LEN 50
#define MAX_PASS_LEN 50
#define KEY_FILE "keyfil"
#define FTOK_ID 65
#define SEM_SERVER "/server_sem"
#define SEM_CLIENT "/client_sem"

// Estructura para la memoria compartida
typedef struct {
    char username[MAX_USER_LEN];
    size_t username_len;
    char password[MAX_PASS_LEN];
    size_t password_len;
    char response[256];
    int authenticated;
    int session_id;
    pid_t client_pid;
    char action; // 'L' para login, 'R' para registro
} SharedData;

// Estructura para usuarios
typedef struct {
    char username[MAX_USER_LEN];
    char password[MAX_PASS_LEN]; // En realidad debería ser un hash
} User;

typedef struct {
    pid_t pid;
    int session_id;
    time_t last_activity;
} ActiveClient;

// Funciones comunes
int encrypt_aes256(const unsigned char *input, int input_len, unsigned char *output, const unsigned char *key);
int decrypt_aes256(const unsigned char *input, int input_len, unsigned char *output, const unsigned char *key);

int validate_password(const char *password);
void generate_session_id(char *session_id, int size);

#endif