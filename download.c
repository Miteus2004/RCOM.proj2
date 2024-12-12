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

    int retrieveCode = retrieveFile(&ftp, url.resource);
    if (retrieveCode < READY2TRANSFER_CODE_A || retrieveCode > READY2TRANSFER_CODE_B) {
        fprintf(stderr, "Error: Invalid response code '%d' for retrieving resource '%s'\n", retrieveCode, url.resource);
        return -1;
    }
    
    if (downloadFile(ftp.control_fd, ftp.data_fd, url.file) <= READY2TRANSFER_CODE_B) {
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

    // Initialize the server address structure
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                // IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip);     // Set IP
    server_addr.sin_port = htons(port);              // Set port

    // Create the socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");           // Detailed error message
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect");                 // Detailed error message
        close(sockfd);                               // Clean up
        return -1;
    }

    printf("Socket connection established to %s:%d\n", ip, port);
    return sockfd;
}


int receiveResponse(int socketfd, char *buffer) {
    char tempBuffer[MAX_LENGTH + 1] = {0};
    char messageBuffer[MAX_LENGTH] = {0};
    int bytesRead, responseCode = 0, initialResponseCode = 0;
    char delimiter;
    bool multiline = false;

    // Initialize buffer
    buffer[0] = '\0';

    while (1) {
        // Read from the socket
        bytesRead = read(socketfd, tempBuffer, MAX_LENGTH);
        if (bytesRead < 0) {
            perror("Failed to read from socket");
            return -1;
        } else if (bytesRead == 0) {
            printf("Connection closed by server\n");
            return -1;
        }

        // Null-terminate the received data
        tempBuffer[bytesRead] = '\0';
        printf("Received message: %s\n", tempBuffer);

        // Process each line in the received buffer
        char *line = strtok(tempBuffer, "\r\n");
        while (line != NULL) {
            // Check the response code and delimiter on the first line
            if (initialResponseCode == 0 && sscanf(line, "%3d%c", &responseCode, &delimiter) >= 2) {
                initialResponseCode = responseCode;
                multiline = (delimiter == '-');
                printf("Response code: %d\n", responseCode);
                printf("Delimiter: %c\n", delimiter);

                if (multiline) {
                    printf("Multiline response detected.\n");
                }
            }

            // Extract the message part of the line (excluding response code and delimiter)
            char *messageStart = line + 4; // Skip the response code and delimiter
            if (*messageStart != '\0') {  // Ensure the line has content beyond the code
                strncat(messageBuffer, messageStart, MAX_LENGTH - strlen(messageBuffer) - 1);
                strncat(messageBuffer, "\n", MAX_LENGTH - strlen(messageBuffer) - 1);
            } else {
                strncat(messageBuffer, "\n", MAX_LENGTH - strlen(messageBuffer) - 1); // Preserve blank lines
            }

            // For multi-line, stop if the termination line is detected
            if (multiline && sscanf(line, "%3d%c", &responseCode, &delimiter) == 2) {
                if (responseCode == initialResponseCode && delimiter == ' ') {
                    multiline = false;
                }
            }

            // Process the next line
            line = strtok(NULL, "\r\n");
        }

        // Exit the loop if not multiline or the end of the response is reached
        if (!multiline) {
            break;
        }
    }

    printf("Final response (text only): %s\n", messageBuffer);

    // Copy the final response into the provided buffer
    strncpy(buffer, messageBuffer, MAX_LENGTH - 1);
    buffer[MAX_LENGTH - 1] = '\0';

    return initialResponseCode;
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
    if (write(socket, "pasv\r\n", 6) < 0) { 
        fprintf(stderr, "Failed to send PASV command\n");
        return -1;
    }

    // Receive the server's response
    int responseCode = receiveResponse(socket, response);
    if (responseCode != PASSIVE_CODE) { 
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
            close(dataSocket);  
            return -1;
        }
    }

    fclose(file);

    // Close the data socket
    if (close(dataSocket) < 0) {
        perror("Error closing data socket");
        return -1;
    }

    if (bytesRead < 0) {
        printf("Error reading from data socket");
        return -1;
    }

    // Confirm file transfer completion
    char response[MAX_LENGTH];
    int responseCode = receiveResponse(controlSocket, response);

    if (responseCode != TRANSFER_COMPLETE_CODE) {
        printf("Error: Transfer not confirmed by server. Response: %s\n", response);
        return -1;
    }

    return responseCode;
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
        if (receiveResponse(controlSocket, response) != GOODBYE_CODE) {
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
    return 0; 
}