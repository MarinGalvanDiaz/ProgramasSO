#include "common.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

ActiveClient active_clients[MAX_CLIENTS];
int active_count = 0;

void update_active_clients(pid_t pid, int session_id) {
    for (int i = 0; i < active_count; i++) {
        if (active_clients[i].pid == pid) {
            active_clients[i].last_activity = time(NULL);
            return;
        }
    }

    if (active_count < MAX_CLIENTS) {
        active_clients[active_count].pid = pid;
        active_clients[active_count].session_id = session_id;
        active_clients[active_count].last_activity = time(NULL);
        active_count++;
    }
}

User users[MAX_USERS];
int user_count = 0;
unsigned char aes_key[32]; // Clave AES-256

void load_users()
{
    FILE *file = fopen("usuarios.txt", "rb");
    if (!file)
    {
        file = fopen("usuarios.txt", "wb");
        fclose(file);
        return;
    }

    user_count = 0;
    while (1)
    {
        int encrypted_len;
        if (fread(&encrypted_len, sizeof(int), 1, file) != 1)
            break;

        unsigned char encrypted[256];
        if (fread(encrypted, 1, encrypted_len, file) != (size_t)encrypted_len)
            break;

        unsigned char decrypted[256];
        int decrypted_len = decrypt_aes256(encrypted, encrypted_len, decrypted, aes_key);
        decrypted[decrypted_len] = '\0';

        sscanf((char *)decrypted, "%s %s", users[user_count].username, users[user_count].password);
        user_count++;

        if (user_count >= MAX_USERS)
            break;
    }

    fclose(file);
}

void save_users()
{
    FILE *file = fopen("usuarios.txt", "wb");
    if (!file)
    {
        printw("Error al abrir archivo de usuarios!\n");
        return;
    }

    for (int i = 0; i < user_count; i++)
    {
        char combined[MAX_USER_LEN + MAX_PASS_LEN + 2];
        snprintf(combined, sizeof(combined), "%s %s", users[i].username, users[i].password);

        unsigned char encrypted[256];
        int encrypted_len = encrypt_aes256((unsigned char *)combined, strlen(combined), encrypted, aes_key);

        fwrite(&encrypted_len, sizeof(int), 1, file);
        fwrite(encrypted, 1, encrypted_len, file);
    }

    fclose(file);
}

int user_exists(const char *username)
{
    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].username, username) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int authenticate_user(const char *username, const char *password)
{
    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0)
        {
            return 1;
        }
    }
    return 0;
}

void add_user(const char *username, const char *password)
{
    if (user_count >= MAX_USERS)
        return;

    strcpy(users[user_count].username, username);
    strcpy(users[user_count].password, password);
    user_count++;
    save_users();
}


void *handle_client(void *arg)
{
    SharedData *shared_data = (SharedData *)arg;
    char decrypted_user[MAX_USER_LEN];
    char decrypted_pass[MAX_PASS_LEN];

    decrypt_aes256(
        (const unsigned char *)shared_data->username,
        shared_data->username_len,
        (unsigned char *)decrypted_user,
        aes_key);

    decrypt_aes256(
        (const unsigned char *)shared_data->password,
        shared_data->password_len,
        (unsigned char *)decrypted_pass,
        aes_key);

    int session_id = rand() % 10000 + 1;

    if (shared_data->action == 'L')
    { // Login
        if (authenticate_user(decrypted_user, decrypted_pass))
        {
            shared_data->authenticated = 1;
            shared_data->session_id = session_id;
            snprintf(shared_data->response, sizeof(shared_data->response),
                     "Autenticación exitosa. ID de sesión: %d", session_id);
        }
        else
        {
            shared_data->authenticated = 0;
            strcpy(shared_data->response, "Usuario o contraseña incorrectos");
        }
    }
    else if (shared_data->action == 'R')
    { // Registro
        if (user_exists(decrypted_user))
        {
            shared_data->authenticated = 0;
            strcpy(shared_data->response, "El usuario ya existe");
        }
        else if (!validate_password(decrypted_pass))
        {
            shared_data->authenticated = 0;
            strcpy(shared_data->response, "La contraseña debe tener al menos 8 caracteres con letras y números");
        }
        else
        {
            add_user(decrypted_user, decrypted_pass);
            shared_data->authenticated = 1;
            shared_data->session_id = session_id;
            snprintf(shared_data->response, sizeof(shared_data->response),
                     "Registro exitoso. ID de sesión: %d", session_id);
        }
    }

    // Actualizar lista de clientes activos
    update_active_clients(shared_data->client_pid, session_id);

    return NULL;
}

void *server_ui_thread(void *arg) {
    initscr();
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);  // Activo
    init_pair(2, COLOR_RED, COLOR_BLACK);    // Inactivo
    init_pair(3, COLOR_CYAN, COLOR_BLACK);   // Título

    cbreak(); noecho(); curs_set(0);
    nodelay(stdscr, TRUE); // No bloquea getch()

    WINDOW *server_win = newwin(20, 60, 1, 1);
    box(server_win, 0, 0);
    mvwprintw(server_win, 0, 2, " SERVIDOR EN EJECUCI\303\263N ");
    wrefresh(server_win);

    while (1) {
        werase(server_win);
        box(server_win, 0, 0);
        wattron(server_win, COLOR_PAIR(3) | A_BOLD);
        mvwprintw(server_win, 0, 2, " SERVIDOR EN EJECUCI\303\263N ");
        wattroff(server_win, COLOR_PAIR(3) | A_BOLD);

        time_t now = time(NULL);
        int row = 2;

        mvwprintw(server_win, row++, 1, "Clientes conectados: %d", active_count);
        mvwprintw(server_win, row++, 1, "PID      Sesi\303\263n    Ultima Actividad");

        for (int i = 0; i < active_count; i++) {
            int seconds = (int)difftime(now, active_clients[i].last_activity);

            if (seconds <= 30)
                wattron(server_win, COLOR_PAIR(1));  // Verde
            else
                wattron(server_win, COLOR_PAIR(2));  // Rojo

            mvwprintw(server_win, row++, 1, "%-8d %-9d %3d seg",
                      active_clients[i].pid,
                      active_clients[i].session_id,
                      seconds);

            wattroff(server_win, COLOR_PAIR(1));
            wattroff(server_win, COLOR_PAIR(2));
        }

        mvwprintw(server_win, row + 1, 1, "Presione 'q' para salir.");
        wrefresh(server_win);

        int ch = getch();
        if (ch == 'q') break;

        usleep(500000); // 0.5 segundos
    }

    delwin(server_win);
    endwin();
    pthread_exit(NULL);
}


int main()
{
    srand(time(NULL)); // Para generación de session_ids

    // Generar o cargar clave AES
    memset(aes_key, 0x2A, sizeof(aes_key));

    load_users();

    // Crear memoria compartida
    FILE *f = fopen(KEY_FILE, "a");
    if (!f)
    {
        perror("No se pudo crear el archivo de clave");
        exit(1);
    }
    fclose(f);

    // Ahora podemos usar ftok con seguridad
    key_t key = ftok(KEY_FILE, FTOK_ID);
    if (key == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }

    if (sizeof(SharedData) == 0)
    {
        perror("Shareddata");
        exit(1);
    }

    int shmid = shmget(key, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }

    SharedData *shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }

    // Crear semáforos (eliminar primero si ya existen)
    sem_unlink(SEM_SERVER);
    sem_unlink(SEM_CLIENT);

    sem_t *sem_server = sem_open(SEM_SERVER, O_CREAT, 0644, 0);
    sem_t *sem_client = sem_open(SEM_CLIENT, O_CREAT, 0644, 0);

    if (sem_server == SEM_FAILED || sem_client == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    // Iniciar interfaz en un hilo separado
    pthread_t ui_thread;
    pthread_create(&ui_thread, NULL, server_ui_thread, NULL);

    printf("Servidor iniciado. PID: %d\n", getpid());
    printf("Memoria compartida y semáforos creados.\n");

    // Bucle principal del servidor
    while (1)
    {
        // Esperar notificación del cliente
        if (sem_wait(sem_server) == -1)
        {
            perror("sem_wait");
            break;
        }

        // Procesar solicitud del cliente
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, shared_data);
        pthread_detach(client_thread);

        // Notificar al cliente que puede continuar
        sem_post(sem_client);
    }

    // Limpieza
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(sem_server);
    sem_close(sem_client);
    sem_unlink(SEM_SERVER);
    sem_unlink(SEM_CLIENT);

    pthread_join(ui_thread, NULL);
    return 0;
}