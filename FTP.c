#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<signal.h>
#include<dirent.h>
#include<sys/stat.h>
#include<pwd.h>
#include<grp.h>
#include<time.h>
#include<sys/utsname.h>
#define user_default "user"
#define pass_default "pass"

int server_fd, client_fd, data_socket_fd;
char CLIENT_IP[INET_ADDRSTRLEN];
// TODO: PUT RELATED CLIENT INFO IN A STRUCT FOR EASY ACCESS
char username[100] = {0};
char password[100] = {0};
int authenticated = 0;
DIR *dirp;
struct dirent *dp;
int data_port;
char client_data_ip[INET_ADDRSTRLEN];
struct utsname sys_info;

void get_client_ip(struct sockaddr_in client_adress){
   // This setup is to retrieve info of client   

   // inet_ntop converts binary IP_adress into a human-readable string
   inet_ntop(AF_INET, &(client_adress.sin_addr), CLIENT_IP, INET_ADDRSTRLEN);
   printf("Client connected %s: %d\n", CLIENT_IP, ntohs(client_adress.sin_port));
}

void handle_signal(int sig){
   printf("\nClosing the server....\n");
   close(server_fd);
   exit(0);
}

void format_file_metadata(const char *filename, char *output, size_t output_size){
   struct stat metadata;
   if(stat(filename, &metadata) < 0){
      perror("Stat failed");
      return;
   }
   char permissions[11];
   permissions[0] = S_ISDIR(metadata.st_mode) ? 'd' : '-';
   permissions[1] = (metadata.st_mode & S_IRUSR) ? 'r' : '-';
   permissions[2] = (metadata.st_mode & S_IWUSR) ? 'w' : '-';
   permissions[3] = (metadata.st_mode & S_IXUSR) ? 'x' : '-';
   permissions[4] = (metadata.st_mode & S_IRGRP) ? 'r' : '-';
   permissions[5] = (metadata.st_mode & S_IWGRP) ? 'w' : '-';
   permissions[6] = (metadata.st_mode & S_IXGRP) ? 'x' : '-';
   permissions[7] = (metadata.st_mode & S_IROTH) ? 'r' : '-';
   permissions[8] = (metadata.st_mode & S_IWOTH) ? 'w' : '-';
   permissions[9] = (metadata.st_mode & S_IXOTH) ? 'x' : '-';
   permissions[10] = '\0';

   struct passwd *pw = getpwuid(metadata.st_uid);
   struct group *gr = getgrgid(metadata.st_gid);

   char mod_time[20];
   strftime(mod_time, sizeof(mod_time), "%b %d %H:%M", localtime(&metadata.st_mtime));

   snprintf(output,
            output_size,
            "%s %ld %s %s %ld %s %s\r\n",
            permissions,
            (long)metadata.st_nlink,
            pw ? pw->pw_name : "unknown",
            gr ? gr->gr_name : "unknown",
            (long)metadata.st_size,
            mod_time,
            filename);
}

int setup_active_connection(){
   struct sockaddr_in client_data_adress;
   client_data_adress.sin_family = AF_INET;
   client_data_adress.sin_port = htons(data_port);
   client_data_adress.sin_addr.s_addr = inet_addr(client_data_ip);
   int data_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if(connect(data_socket_fd, (struct sockaddr*)&client_data_adress, sizeof(client_data_adress)) < 0){
      perror("Data connection failed");
      close(data_socket_fd);
      return -1;
   }
   else {
      return data_socket_fd;
   }

}

void send_response(int client_fd, const char *response) {
   send(client_fd, response, strlen(response), 0);
}

int main(int argc, char *argv[]){
   printf("v2.2.1\n");
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
   } else {
      printf("Socket created \n");
   }

   // Binds the socket to a port
   if(bind(server_fd, (struct sockaddr *)&server_adress, sock_len) < 0){
      perror("Socket binding failed");
      close(server_fd);
      exit(EXIT_FAILURE);
   } else {
      printf("Socket binded to port : %d\n", PORT);
   }

   listen(server_fd, 5);
   printf("Listening on port : %d\n", PORT);

   while (1) {
      struct sockaddr_in client_adress;
      socklen_t client_len = sizeof(client_adress);
      client_fd = accept(server_fd, (struct sockaddr *)&client_adress, &client_len);
      if(client_fd < 0){
         perror("Failed to accept incoming connection");
         exit(EXIT_FAILURE);
      } else {
         get_client_ip(client_adress);
         send_response(client_fd, "220 Welcome to the Server\r\n");




         // recieve client input
         char BUFFER[1024];

         while(1){
            ssize_t bytes_received = recv(client_fd, BUFFER, sizeof(BUFFER) - 1, 0);
            if(bytes_received){
               BUFFER[bytes_received] = '\0';
               // printf("RAW BUFFER : %s\n", BUFFER);

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
                  shutdown(client_fd, SHUT_RDWR);
                  close(client_fd);
               } else if(strncmp(BUFFER, "PORT ", 5) == 0){
                  int h1, h2, h3, h4, p1, p2;
                  sscanf(BUFFER +5, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
                  snprintf(client_data_ip, sizeof(client_data_ip), "%d.%d.%d.%d",h1, h2, h3, h4);
                  data_port = p1 * 256 + p2;
                  printf("Client IP : %s, Data Port : %d\n", client_data_ip, data_port);
                  send_response(client_fd, "200 Port command succesfull\r\n");
                  
               } else if(strncmp(BUFFER, "LIST", 4) == 0){
               
                  if(!authenticated){
                     send_response(client_fd, "530 Not Authenticated\r\n");
                  }
                  data_socket_fd = setup_active_connection();
                  if(data_socket_fd < 0){
                     send_response(client_fd, "425 Can't open data connection\r\n");
                     continue;

                  }
                  send_response(client_fd, "150 Opening data connection\r\n");

                  if((dirp = opendir(".")) == NULL){
                     close(data_socket_fd);
                     send_response(client_fd, "550 Permission denied\r\n");
                  } 
                  
                  while((dp = readdir(dirp)) != NULL){
                     if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0){
                        continue;
                     }
                     char entry[256];
                     format_file_metadata(dp->d_name, entry, sizeof(entry));
                     send(data_socket_fd, entry, strlen(entry), 0);
                  }
                  
                  closedir(dirp);
                  close(data_socket_fd);
                  send_response(client_fd, "226 Directory Send OK\r\n");
               } else if(strncmp(BUFFER, "SYST", 4) == 0){
                     send_response(client_fd, "215 UNIX Type: L8\r\n");
               } else if(strncmp(BUFFER, "FEAT", 4) == 0){
                  send_response(client_fd, "211 No Features\r\n");
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