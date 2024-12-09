#include "download.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid call to download.\n");
        return -1;
    }

    URL url;
    memset(&url, 0, sizeof(URL));
    if (parseURL(argv[1], &url) != 0) {
        fprintf(stderr, "Parse error. Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        return -1;
    }

    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", url.host, url.resource, url.file, url.user, url.password, url.ip);

    FTP ftp;
    char response[MAX_LENGTH];
    ftp.control_fd = establishSocket(url.ip, FTP_PORT);
    if (ftp.control_fd < 0 || receiveResponse(ftp.control_fd, response) != READY2AUTH_CODE) {
        fprintf(stderr, "Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        return -1;
    }

    if (loginToServer(&ftp, url.user, url.password) != LOGINSUCCESS_CODE) {
        fprintf(stderr, "Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        return -1;
    }
    
    int port;
    if (enablePassiveMode(ftp.control_fd, &port, url.ip) != PASSIVE_CODE) {
        fprintf(stderr, "Passive mode failed\n");
        return -1;
    }

    ftp.data_fd = establishSocket(url.ip, port);
    if (ftp.data_fd < 0) {
        fprintf(stderr, "Socket to '%s:%d' failed\n", url.ip, port);
        return -1;
    }

    if (retrieveFile(&ftp, url.resource) != READY2TRANSFER_CODE) {
        fprintf(stderr, "Unknown resource '%s' in '%s:%d'\n", url.resource, url.ip, port);
        return -1;
    }

    if (downloadFile(ftp.control_fd, ftp.data_fd, url.file) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "Error transferring file '%s' from '%s:%d'\n", url.file, url.ip, port);
        return -1;
    }

    if (disconnectFromServer(&ftp) != 0) {
        fprintf(stderr, "Sockets close error\n");
        return -1;
    }

    return 0;
}





















int parseURL(char *input, URL *url) {
    const char *pattern = "^ftp://(([^:@/]+)(:([^@/]+))?@)?([^/]+)/(.+)$";

    regex_t regex;
    regmatch_t matches[7];  

    // Compile the regular expression
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex.\n");
        return -1;
    }

    // Execute the regular expression on the URL string
    if (regexec(&regex, input, 7, matches, 0) != 0) {
        fprintf(stderr, "Invalid URL format.\n");
        return -1;
    }

    // Extract user (if present)
    if (matches[2].rm_so != -1) {
        strncpy(url->user, input + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
        url->user[matches[2].rm_eo - matches[2].rm_so] = '\0';
    } else {
        strncpy(url->user, "anonymous", MAX_LENGTH - 1);
        url->user[MAX_LENGTH - 1] = '\0';
    }

    // Extract password (if present)
    if (matches[4].rm_so != -1) {
        strncpy(url->password, input + matches[4].rm_so, matches[4].rm_eo - matches[4].rm_so);
        url->password[matches[4].rm_eo - matches[4].rm_so] = '\0';
    } else {
        strncpy(url->password, "password", MAX_LENGTH - 1);
        url->password[MAX_LENGTH - 1] = '\0';
    }

    // Extract host
    if (matches[5].rm_so != -1) {
        strncpy(url->host, input + matches[5].rm_so, matches[5].rm_eo - matches[5].rm_so);
        url->host[matches[5].rm_eo - matches[5].rm_so] = '\0';
    }

    // Extract resource path
    if (matches[6].rm_so != -1) {
        strncpy(url->resource, input + matches[6].rm_so, matches[6].rm_eo - matches[6].rm_so);
        url->resource[matches[6].rm_eo - matches[6].rm_so] = '\0';
    }

    // Extract file from the resource
    char *last_slash = strrchr(url->resource, '/');
    if (last_slash && *(last_slash + 1) != '\0') {
        strncpy(url->file, last_slash + 1, MAX_LENGTH - 1);
        url->file[MAX_LENGTH - 1] = '\0';
    } else {
        strncpy(url->file, url->resource, MAX_LENGTH - 1);
        url->file[MAX_LENGTH - 1] = '\0';
    }

    // Resolve host to IP address
    struct hostent *h;
    if ((h = gethostbyname(url->host)) == NULL) {
        fprintf(stderr, "Failed to resolve hostname '%s'.\n", url->host);
        regfree(&regex);
        return -1;
    }
    strncpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)), MAX_LENGTH - 1);

    // Free regex memory
    regfree(&regex);

    return 0;
}

int establishSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *) &server_addr, sizeof(server_addr)); // Initialize server address
    server_addr.sin_family = AF_INET;                 // Use IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip);      // Set server IP
    server_addr.sin_port = htons(port);               // Set server port

    // Create a socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to create socket to '%s:%d'\n", ip, port);
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to connect to '%s:%d'\n", ip, port);
        return -1;
    }

    return sockfd;
}

int receiveResponse(int socketfd, char *buffer) {
    char tempBuffer[CODE_SIZE + 1] = {0};
    char messageBuffer[MAX_LENGTH] = {0};
    int bytesRead, totalBytes = 0, responseCode;
    bool multiline = false;

    while (1) {
        // Read response code 
        bytesRead = read(socketfd, tempBuffer, CODE_SIZE);
        if (bytesRead < CODE_SIZE) {
            printf("Error: Failed to read response code. Bytes read: %d\n", bytesRead);
            return -1; 
        }
        tempBuffer[CODE_SIZE] = '\0'; // Null-terminate for safety
        responseCode = atoi(tempBuffer);  // Convert to integer
        char delimiter = tempBuffer[3];
        printf("Response code: %d\n", responseCode);

        // Check for multiline response
        if (delimiter == '-') {
            multiline = true;
            printf("Multiline response detected.\n");
        } else if (delimiter == ' ') {
            multiline = false;
        } else {
            printf("Invalid response delimiter: %c\n", delimiter);
            return -1;
        }

        // Read the rest of the message
        while (1) {
            bytesRead = read(socketfd, &messageBuffer[totalBytes], 1);
            if (bytesRead <= 0) {
                printf("Error: Failed to read message line. Bytes read: %d\n", bytesRead);
                return -1;
            }
            // Break on newline
            if (messageBuffer[totalBytes] == '\n') {
                totalBytes++;
                messageBuffer[totalBytes] = '\0';
                break;
            }
            totalBytes++;
        }

        printf("Message line read: %s\n", messageBuffer);

        // If not a multiline response or the final line matches the response code, break
        if (!multiline || strncmp(messageBuffer, tempBuffer, CODE_SIZE) == 0) {
            break;
        }

        totalBytes = 0; // Reset for reading the next line in multiline responses
    }

    // Copy the final response into the provided buffer
    strncpy(buffer, messageBuffer, MAX_LENGTH - 1);
    buffer[MAX_LENGTH - 1] = '\0';
    return responseCode;
}


int loginToServer(FTP* ftp, char* username, char* password) {
    return authenticate(ftp->control_fd, username, password);
}

int authenticate(int socket, const char *user, const char *pass) {
    char userCmd[MAX_LENGTH], passCmd[MAX_LENGTH], response[MAX_LENGTH];

// Prepare USER command
snprintf(userCmd, sizeof(userCmd), "user %s\r\n", user); 
if (write(socket, userCmd, strlen(userCmd)) < 0) {
    perror("Failed to send USER command");
    return -1;
}

// Receive response
int userResponse = receiveResponse(socket, response);
if (userResponse != 331) { 
    fprintf(stderr, "Server did not accept username '%s'. Response: %d\n", user, userResponse);
    return -1;
}

// Prepare PASS command
snprintf(passCmd, sizeof(passCmd), "pass %s\r\n", pass); 
if (write(socket, passCmd, strlen(passCmd)) < 0) {
    perror("Failed to send PASS command");
    return -1;
}

// Receive final login response
return receiveResponse(socket, response);
}

int enablePassiveMode(const int socket, int *port, char *ip) {
    char response[MAX_LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;

    // Send the PASV command
    if (write(socket, "pasv\r\n", 6) < 0) { // Ensure CRLF
        fprintf(stderr, "Failed to send PASV command\n");
        return -1;
    }

    // Receive the server's response
    int responseCode = receiveResponse(socket, response);
    if (responseCode != PASSIVE_CODE) { // 227 is the typical code for PASV
        fprintf(stderr, "Failed to enter passive mode. Response: %s\n", response);
        return -1;
    }

    // Parse the IP and port from the response
    if (sscanf(response, "Entering Passive Mode (%d,%d,%d,%d,%d,%d).",
               &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6) {
        fprintf(stderr, "Failed to parse passive mode response: %s\n", response);
        return -1;
    }

    // Validate the IP
    int server_ip1, server_ip2, server_ip3, server_ip4;
    if (sscanf(ip, "%d.%d.%d.%d", &server_ip1, &server_ip2, &server_ip3, &server_ip4) != 4) {
        fprintf(stderr, "Invalid server IP format: %s\n", ip);
        return -1;
    }

    if (ip1 != server_ip1 || ip2 != server_ip2 || ip3 != server_ip3 || ip4 != server_ip4) {
        fprintf(stderr, "IP addresses do not match: server (%d.%d.%d.%d), response (%d.%d.%d.%d)\n",
                server_ip1, server_ip2, server_ip3, server_ip4, ip1, ip2, ip3, ip4);
        return -1;
    }

    // Calculate the data port
    *port = (port1 * 256) + port2;
    printf("Passive mode enabled: data port = %d\n", *port);
    return PASSIVE_CODE;
}


int retrieveFile(FTP* ftp, char* fileName) {
    return requestFile(ftp->control_fd, fileName);
}

int requestFile(const int controlSocket, const char *resourcePath) {
    char command[MAX_LENGTH];
    char serverResponse[MAX_LENGTH];

    // Prepare and send the retr command
    snprintf(command, sizeof(command), "retr %s\r\n", resourcePath);
    if (write(controlSocket, command, strlen(command)) < 0) {
        perror("Failed to send RETR command");
        return -1;
    }


    // Wait for the server's response
    int responseCode = receiveResponse(controlSocket, serverResponse);
    if (responseCode < 0) {
        printf("Error retrieving resource: %s\n", resourcePath);
        return -1;
    }

    return responseCode;
}


int downloadFile(const int controlSocket, const int dataSocket, const char *fileName) {
    FILE *file = fopen(fileName, "wb");
    if (file == NULL) {
        printf("Error opening file for download");
        return -1;
    }

    char buffer[MAX_LENGTH];
    int bytesRead;

    // Read data from the data socket and write to the file
    while ((bytesRead = read(dataSocket, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytesRead, file) != bytesRead) {
            printf("Error writing to file");
            fclose(file);
            return -1;
        }
    }

    fclose(file);

    if (bytesRead < 0) {
        printf("Error reading from data socket");
        return -1;
    }

    // Confirm file transfer completion
    char response[MAX_LENGTH];
    return receiveResponse(controlSocket, response);
}

int disconnectFromServer(FTP* ftp) {
    return terminateConnection(ftp->control_fd, ftp->data_fd);
}

int terminateConnection(const int controlSocket, const int dataSocket) {
    char response[MAX_LENGTH] = {0};
    
    // Send the QUIT command to the server
    if (controlSocket >= 0) {
        if (write(controlSocket, "quit\r\n", 6) != 6) {
            perror("Error sending QUIT command");
            return -1;
        }
        
        // Read the server's response to QUIT
        if (receiveResponse(controlSocket, response) < 0) {
            fprintf(stderr, "Error reading response to QUIT command\n");
        } else {
            printf("Server response to QUIT: %s\n", response);
        }

        // Close the control socket
        if (close(controlSocket) < 0) {
            perror("Error closing control socket");
            return -1;
        }
    }

    // Close the data socket, if it exists
    if (dataSocket >= 0) {
        if (close(dataSocket) < 0) {
            perror("Error closing data socket");
            return -1;
        }
    }

    return 0; 
}



