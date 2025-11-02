#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>

#define MAX_MSG 256

struct input_s{
        int cod_req;
        char path[MAX_MSG];
        int mtime;
};

enum {
        EXIT,
        POST,
        DELETE,
        GET,
        ERROR
};

typedef struct input_s input_t;

void parse_input(char* input, input_t* userInput){
        if(input[0] == '\0'){
                userInput->cod_req = ERROR;
                return;
        }

        char* token = strtok(input, " ");
        int i = 0;

        userInput->cod_req = ERROR;
        userInput->mtime = 0;

        if(!strcmp(token, "GET"))
                userInput->cod_req = GET;
        if(!strcmp(token, "POST"))
                userInput->cod_req = POST;
        if(!strcmp(token, "DELETE"))
                userInput->cod_req = DELETE;
        if(!strcmp(token, "EXIT"))
                userInput->cod_req = EXIT;

        if(userInput->cod_req == ERROR){
                printf("ERROR: command not supported.\n");
                return;
        }

        while((token = strtok(NULL, " "))){
                if(i > 0){
                        printf("ERROR: too many arguments.\n");
                        userInput->cod_req = ERROR;
                        return;
                }

                strcpy(userInput->path, token);

                i++;
        }

        return;
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

int main(int argc, char **argv) {
        if (argc !=3) {
                printf("Usage: %s <dst_ip> <dst_port>\n", argv[0]);
                return 0;
        }

        char msg[MAX_MSG], resp[MAX_MSG], input[MAX_MSG], arg_resp[MAX_MSG], filesize[MAX_MSG], cod_res[MAX_MSG];
        int ns, nr;
        input_t userInput;
        struct stat file_stat;
        
        
        while(1){
                // socket
                int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (cfd == -1) {
                        perror("socket()");
                        return -1;
                }
                struct sockaddr_in saddr;
                saddr.sin_port = htons(atoi(argv[2]));
                saddr.sin_family = AF_INET;
                saddr.sin_addr.s_addr = inet_addr(argv[1]);

                // connet
                if (connect(cfd, (struct sockaddr*) &saddr, sizeof(struct sockaddr_in)) == -1) {
                        perror("connect()");
                        return -1;
                }

                printf(">>");
                fgets(input, MAX_MSG, stdin);
                input[strlen(input) - 1] = '\0';
        
                parse_input(input, &userInput);

                if(userInput.cod_req == EXIT){
                        close(cfd);
                        break;
                }

                if(userInput.cod_req == ERROR)
                        continue;

                if(userInput.cod_req == GET){
                        if(lstat(userInput.path, &file_stat) == -1)
                                userInput.mtime = 0; //Arquivo não existe para o usuário
                        else
                                userInput.mtime = file_stat.st_mtime;
                }

                if(userInput.cod_req == POST){
                        if(lstat(userInput.path, &file_stat) == -1){
                                printf("ERROR: file not found to send.\n");
                                continue;
                        }

                        userInput.mtime = file_stat.st_size;
                }

                //write
                // Manda a primeira requisicao
                bzero(msg, MAX_MSG);
                sprintf(msg, "%d %s %d", userInput.cod_req, userInput.path, userInput.mtime);
                ns = write(cfd, msg, strlen(msg));
                if (ns == -1) {
                        perror("write()");
                        return -1;
                }
                
                // read
                if(userInput.cod_req == POST){
                        send_file(cfd, userInput.path);
                        printf("File uploaded.\n");

                        bzero(resp, MAX_MSG);
                        nr = read(cfd, resp, MAX_MSG);
                        if (nr == -1) {
                                perror("read()");
                                return -1;
                        }
                        printf("%s", resp);
                }
                else{
                        bzero(resp, MAX_MSG);
                        nr = read(cfd, resp, MAX_MSG);
                        if (nr == -1) {
                                perror("read()");
                                return -1;
                        }
                        printf("%s", resp);

                        if(userInput.cod_req == GET){
                                sscanf((char*)resp, "%s %s %s", cod_res, arg_resp, filesize); 
                                
                                if(atoi(cod_res) == 200){
                                        if(recv_file(cfd, userInput.path, atoi(filesize)) == 0)
                                                printf("File downloaded.\n");
                                }
                        }
                }

                close(cfd);
        }

        return 0;
}