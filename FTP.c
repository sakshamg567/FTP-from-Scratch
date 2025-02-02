#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<signal.h>
#define user_default "user"
#define pass_default "pass"
int server_fd;

void handle_signal(int sig){
   printf("\nClosing the server....\n");
   close(server_fd);
   exit(0);
}

void send_response(int client_fd, const char *response) {
   send(client_fd, response, strlen(response), 0);
}

int main(int argc, char *argv[]){
   signal(SIGINT, handle_signal);
   int PORT = atoi(argv[1]);
   struct sockaddr_in server_adress;
   server_adress.sin_family = AF_INET;  // AF_INET is the adress family of IPv4 adresses
   server_adress.sin_port = htons(PORT);
   server_adress.sin_addr.s_addr = INADDR_ANY;

   socklen_t sock_len = sizeof(server_adress);

   // Creates a socket connection of type TCP, with IPv4 domain
   server_fd = socket(AF_INET, SOCK_STREAM, 0);  
   if(server_fd < 0){
      perror("Socket creation failed");
      close(server_fd);
      exit(EXIT_FAILURE);
   } 
   else {
      printf("Socket created \n");
   }

   // Binds the socket to a port
   if(bind(server_fd, (struct sockaddr *)&server_adress, sock_len) < 0){
      perror("Socket binding failed");
      close(server_fd);
      exit(EXIT_FAILURE);
   }
   else {
      printf("Socket binded to port : %d\n", PORT);
   }
   listen(server_fd, 5);
   printf("Listening on port : %d\n", PORT);

   while (1) {
      struct sockaddr_in client_adress;
      socklen_t client_len = sizeof(client_adress);
      int client_fd = accept(server_fd, (struct sockaddr *)&client_adress, &client_len);
      if(client_fd < 0){
         perror("Failed to accept incoming connection");
         exit(EXIT_FAILURE);
      }
      else {
         // This setup is to retrieve info of client
         char CLIENT_IP[INET_ADDRSTRLEN];
         
         // inet_ntop converts binary IP_adress into a human-readable string

         inet_ntop(AF_INET, &(client_adress.sin_addr), CLIENT_IP, INET_ADDRSTRLEN);
         printf("Client connected %s: %d\n", CLIENT_IP, ntohs(client_adress.sin_port));
         send_response(client_fd, "220 Welcome to the Server\r\n");

         char username[100] = {0};
         char password[100] = {0};
         int authenticated = 0;

         // recieve client input
         char BUFFER[1024];
         while(1){
            ssize_t bytes_received = recv(client_fd, BUFFER, sizeof(BUFFER) - 1, 0);
            if(bytes_received){
               BUFFER[bytes_received] = '\0';

               if(strncmp(BUFFER, "USER ", 5) == 0){
                  strncpy(username, BUFFER + 5, sizeof(username) - 1);
                  username[strcspn(username, "\r\n")] = '\0';
                  printf("Username : %s\n", username);
                  send_response(client_fd, "331 Please specify password\r\n");
               } else if(strncmp(BUFFER, "PASS ", 5) == 0){
                  strncpy(password, BUFFER + 5, sizeof(username) - 1);
                  password[strcspn(password, "\r\n")] = '\0';
                  printf("Password : %s\n", password);

                  if(strcmp(username, "anon") == 0){
                     authenticated = 1;
                     send_response(client_fd, "230 Login Succesful\r\n");
                  } else if(strcmp(username, user_default) == 0 && strcmp(password, pass_default) == 0){
                     authenticated = 1;
                     send_response(client_fd, "230 Login Succesful\r\n");
                  } else {
                     send_response(client_fd, "530 Incorrect credentials\r\n");
                  }
                  
               } else if(strncmp(BUFFER, "QUIT", 4) == 0){
                  send_response(client_fd, "221 Good Bye\r\n");
                  close(server_fd);
               } else {
                  send_response(client_fd, "500 Unknown command");
               }
            }

            else{
               printf("Client disconnected");
               shutdown(client_fd, SHUT_RDWR);
               close(client_fd);
               break;
            }
         }
      }
   }
   return 0;
}