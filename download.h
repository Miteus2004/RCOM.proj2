#ifndef  download_H
#define  download_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <stdbool.h>
#include <regex.h>

#define MAX_LENGTH 500
#define CODE_SIZE 4
#define FTP_PORT 21
#define READY2AUTH_CODE 220
#define READY2PASS_CODE 331
#define LOGINSUCCESS_CODE 230
#define PASSIVE_CODE 227
#define READY2TRANSFER_CODE 150
#define TRANSFER_COMPLETE_CODE 226
#define GOODBYE_CODE 221
#define RESPCODE_REGEX "%d"


typedef struct {
    char host[MAX_LENGTH];
    char resource[MAX_LENGTH];
    char file[MAX_LENGTH];
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char ip[MAX_LENGTH];
} URL;

typedef struct {
    int control_fd;
    int data_fd;
} FTP;

int parseURL(char *input, URL *url);
int establishSocket(char *ip, int port);
int receiveResponse(const int socket, char* buffer);
int loginToServer(FTP* ftp, char* username, char* password);
int authenticate(const int socket, const char* user, const char* pass);
int enablePassiveMode(const int socket, int *port, char *ip);
int retrieveFile(FTP* ftp, char* fileName);
int requestFile(const int socket, const char *resource);
int downloadFile(const int controlSocket, const int dataSocket, const char *filename);
int disconnectFromServer(FTP* ftp);
int terminateConnection(const int controlSocket, const int dataSocket);
#endif