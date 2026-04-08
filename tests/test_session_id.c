#include <stdio.h>
#include <string.h>
#include <openssl/rand.h>

static void generate_session_id(char *session_id) {
    unsigned char random_bytes[32];
    RAND_bytes(random_bytes, 32);
    
    for (int i = 0; i < 32; i++) {
        sprintf(session_id + (i * 2), "%02x", random_bytes[i]);
    }
    session_id[64] = '\0';
}

int main(void) {
    char session_id[65];
    generate_session_id(session_id);
    
    printf("Session ID: %s\n", session_id);
    printf("Length: %zu\n", strlen(session_id));
    
    return 0;
}
