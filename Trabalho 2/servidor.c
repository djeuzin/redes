// RAFAEL FREIRE MACHADO GONCALVES RA: 163977
// LUCAS ALVES NASCIMENTO MARQUES RA: 163911
// MARCELO DE CARVALHO MACHADO RA: 163934

// ENTREGA PARCIAL

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>

#define MAX_REQ 64536
#define MAX_CONN 1000

struct targs {
    pthread_t tid;
    int cfd;
    struct sockaddr_in caddr;
};
typedef struct targs targs;

targs tclients[MAX_CONN + 3];

void init(targs *tclients, int n){
    int i;
    for (i = 0; i< MAX_CONN + 3; i++) {
        tclients[i].cfd = -1;
    }
}

// Função para enviar arquivo ao cliente
int send_file(int cfd, const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return -1;
    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (send(cfd, buf, n, 0) != n) {
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

// Função para receber arquivo do cliente
int recv_file(int cfd, const char *filename, int filesize) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    char buf[1024];
    int received = 0, n;
    while (received < filesize) {
        n = recv(cfd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        write(fd, buf, n);
        received += n;
    }
    close(fd);
    return received == filesize ? 0 : -1;
}

// Função para obter data de modificação do arquivo
time_t get_file_mtime(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_mtime;
    return 0;
}

void *handle_client(void *args) {
    int cfd = *(int *)args;
    // recv
    int nr;
    unsigned char requisicao[MAX_REQ];
    bzero(requisicao, MAX_REQ);
    nr = recv(tclients[cfd].cfd, requisicao, MAX_REQ, 0);
    if (nr < 0) {
        perror("erro no recv()");
        return NULL;
    }

    // Parse comando
    char cmd[16], filename[256], extra[256];
    int parsed = sscanf((char*)requisicao, "%15s %255s %255s", cmd, filename, extra);

    char resposta[512];
    int status = 200;

    if (strcmp(cmd, "3") == 0 && parsed >= 2) {
        time_t client_mtime = 0;
        if (parsed == 3) client_mtime = atol(extra);

        time_t server_mtime = get_file_mtime(filename);
        if (server_mtime == 0) {
            status = 404;
            sprintf(resposta, "404 File Not Found\n");
            send(cfd, resposta, strlen(resposta), 0);
        } else if (client_mtime >= server_mtime) {
            status = 304;
            sprintf(resposta, "304 Not Modified\n");
            send(cfd, resposta, strlen(resposta), 0);
        } else {
            sprintf(resposta, "200 OK %ld\n", server_mtime);
            send(cfd, resposta, strlen(resposta), 0);
            send_file(cfd, filename);
        }
    } else if (strcmp(cmd, "1") == 0 && parsed >= 3) {
        int filesize = atoi(extra);
        if (recv_file(cfd, filename, filesize) == 0) {
            status = 200;
            sprintf(resposta, "200 OK\n");
        } else {
            status = 500;
            sprintf(resposta, "500 Error Writing File\n");
        }
        send(cfd, resposta, strlen(resposta), 0);
    } else if (strcmp(cmd, "2") == 0 && parsed >= 2) {
        if (remove(filename) == 0) {
            status = 200;
            sprintf(resposta, "200 OK\n");
        } else {
            status = 404;
            sprintf(resposta, "404 File Not Found\n");
        }
        send(cfd, resposta, strlen(resposta), 0);
    } else {
        status = 400;
        sprintf(resposta, "400 Bad Request\n");
        send(cfd, resposta, strlen(resposta), 0);
    }

    // close
    close(tclients[cfd].cfd);
    tclients[cfd].cfd = -1;
    return NULL;
}

int main(int argc, char **argv) {

    if (argc != 2){
        printf("Uso: %s <porta> \n", argv[0]);
        return 0;
    }

    init(tclients, MAX_CONN+3);

    struct sockaddr_in saddr;

    // socket
    int sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // bind
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sl, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) < 0) {
        perror("erro no bind()");
        return -1;
    }

    if (listen(sl, 1000) < 0) {
        perror("erro no listen()");
        return -1;
    }

    // accept
    int cfd, addr_len, i;
    struct sockaddr_in caddr;
    addr_len = sizeof(struct sockaddr_in);

    while(1) {

        cfd = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);
        if (cfd == 1) {
            perror("erro no accept()");
            return -1;
        }
        tclients[cfd].cfd = cfd;
        tclients[cfd].caddr = caddr;
        pthread_create(&tclients[cfd].tid, NULL, handle_client, (void*)&cfd);
    }
    for (i = 0; i< MAX_CONN + 3; i++) {
        if (tclients[i].cfd == -1) continue;
        pthread_join(tclients[i].tid, NULL);
    }
    return 0;

}