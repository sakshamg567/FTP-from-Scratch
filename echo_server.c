#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdio.h>

int main(){
   int server_fd, PORT = 8080;
   struct sockaddr_in server_adress;
   server_adress.sin_family = AF_INET;  // AF_INET is the adress family of IPv4 adresses
   server_adress.sin_port = htons(PORT);
   server_adress.sin_addr.s_addr = INADDR_ANY;

   socklen_t sock_len = sizeof(server_adress);

   // Creates a socket connection of type TCP, with IPv4 domain
   server_fd = socket(AF_INET, SOCK_STREAM, 0);  
   if(server_fd < 0){
      perror("Socket creation failed");
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
      }
   }
   close(server_fd);
   return 0;
}