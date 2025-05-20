#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

int main() {
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
