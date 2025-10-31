#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define CMD_SIZE 450
#define PATH_SIZE 256
#define MAX_MSG 1024

enum {
        EXIT,
        PUT,
        DELETE,
        GET,
        ERROR
};

struct input_s{
        char command[CMD_SIZE];
        char path[PATH_SIZE];
        int arg;
        int code;
};

typedef struct input_s input_t;
typedef input_t* input_p;

int parse_cmd(input_p);

int main(int argc, char **argv) {
        if (argc !=3) {
                printf("Usage: %s <dst_ip> <dst_port>\n", argv[0]);
                return 1;
        }

        struct stat file_stat;
        FILE* source;
        input_t userInput;
        int code;
        userInput.code = 1;
        char msg[MAX_MSG], resp[MAX_MSG];
        int ns, nr;

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

        while(code != EXIT){
                printf(">>");
                fgets(userInput.command, CMD_SIZE, stdin);

                code = parse_cmd(&userInput);

                if(code == EXIT)
                        break;

                if(code == GET){
                        if(lstat(userInput.path, &file_stat) == -1)
                                userInput.arg = 0; // Arquivo não existe para o usuário
                        else
                                userInput.arg = file_stat.st_mtime;
                }

                if(userInput.code == ERROR){
                        printf("Supported commands: GET, PUT and DELETE\n");
                        continue;
                }

                bzero(msg, MAX_MSG);
                sprintf(msg, "%d %s %d", userInput.code, userInput.path, userInput.arg);

                // write
                ns = write(cfd, msg, strlen(msg));
                if (ns == -1) {
                        perror("write()");
                        return -1;
                }
                // read
                bzero(resp, MAX_MSG);
                nr = read(cfd, resp, MAX_MSG);
                if (nr == -1) {
                        perror("read()");
                        return -1;
                }
                printf("Resposta do servidor: %s\n", resp);

        }

        // close
        close(cfd);
        return 0;
}

int parse_cmd(input_p userInput){
        char* token = strtok(userInput->command, " ");
       
        int i = 0;

        if(!strcmp(token, "GET"))
                userInput->code = GET;
        else if(!strcmp(token, "DELETE"))
                userInput->code = DELETE;
        else if(!strcmp(token, "PUT"))
                userInput->code = PUT;
        else if(!strcmp(token, "EXIT"))
                return EXIT;
        else
                return ERROR;
   

        while((token = strtok(NULL, " "))){
                i++;
                if(i > 1){
                        printf("Error: too many arguments.\n");
                        return ERROR;
                }

                strcpy(userInput->path, token);
        }

        return userInput->code;
}