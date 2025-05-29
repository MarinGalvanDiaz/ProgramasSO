#include "common.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <ctype.h>

void handle_openssl_error() {
    ERR_print_errors_fp(stderr);
    exit(1);
}

int encrypt_aes256(const unsigned char *input, int input_len, unsigned char *output, const unsigned char *key) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handle_openssl_error();
    
    unsigned char iv[16] = {0}; // IV fijo para pruebas, idealmente aleatorio y enviado junto
    
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handle_openssl_error();
    
    if(1 != EVP_EncryptUpdate(ctx, output, &len, input, input_len))
        handle_openssl_error();
    ciphertext_len = len;
    
    if(1 != EVP_EncryptFinal_ex(ctx, output + len, &len))
        handle_openssl_error();
    ciphertext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int decrypt_aes256(const unsigned char *input, int input_len, unsigned char *output, const unsigned char *key) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handle_openssl_error();
    
    unsigned char iv[16] = {0}; // Igual que en cifrado
    
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handle_openssl_error();
    
    if(1 != EVP_DecryptUpdate(ctx, output, &len, input, input_len))
        handle_openssl_error();
    plaintext_len = len;
    
    if(1 != EVP_DecryptFinal_ex(ctx, output + len, &len))
        handle_openssl_error();
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

int validate_password(const char *password) {
    int length = strlen(password);
    if (length < 8) return 0;
    
    int has_letter = 0;
    int has_digit = 0;
    
    for (int i = 0; i < length; i++) {
        if (isalpha(password[i])) has_letter = 1;
        if (isdigit(password[i])) has_digit = 1;
    }
    
    return has_letter && has_digit;
}

void get_username(WINDOW *win, int y, int x, char *buffer, int max_len) {
    int pos = 0, ch;
    keypad(win, TRUE);
    curs_set(1);
    echo();  // Para mostrar el texto mientras escribe
    wmove(win, y, x);

    while (1) {
        ch = wgetch(win);
        if (ch == '\n' || ch == '\r') break;

        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && pos > 0) {
            pos--;
            buffer[pos] = '\0';
            mvwaddch(win, y, x + pos, ' ');
            wmove(win, y, x + pos);
        } else if (pos < max_len - 1) {
            // Validar que sea letra o número:
            if ((ch >= 'a' && ch <= 'z') || 
                (ch >= 'A' && ch <= 'Z') || 
                (ch >= '0' && ch <= '9')) {
                buffer[pos++] = ch;
                mvwaddch(win, y, x + pos - 1, ch);
            } else {
                // Opcional: sonido de error o ignora la tecla
                beep();
            }
        }
        wrefresh(win);
    }
    buffer[pos] = '\0';
    curs_set(0);
    noecho();
}
