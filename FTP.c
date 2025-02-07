#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/utsname.h>

// --- Constants and Macros ---
#define USER_DEFAULT "user"
#define PASS_DEFAULT "pass"

#define BIN 0
#define ASC 1

// FTP Status Codes (using enums for better readability)
typedef enum
{
   FTP_DATA_OPEN = 150,
   FTP_OK = 200,
   FTP_INFO = 211,
   FTP_SYST = 215,
   FTP_NEW_CONNECT = 220,
   FTP_GOODBYE = 221,
   FTP_TRANSFER_COMPLETE = 226,
   FTP_LOGIN_SUCESS = 230,
   FTP_USER_OK_PASS_REQ = 331,
   FTP_DATA_CONNECTION_FAIL = 425,
   FTP_UNKNOWN_COMMAND = 500,
   FTP_SYNTAX_ERROR = 501,
   FTP_AUTH_ERROR = 530,
   FTP_FILE_ERROR = 550
} FTPStatusCode;

// --- Client Context Structure ---
typedef struct
{
   char username[100];
   char password[100];
   int authenticated;
   int data_port;
   char client_data_ip[INET_ADDRSTRLEN];
   int transfer_mode;
   int data_socket_fd;
} ClientContext;

// --- Global Variables (Minimize these if possible, ClientContext helps) ---
int server_fd, client_fd; // Still global for signal handler and main loop
char CLIENT_IP[INET_ADDRSTRLEN];

// --- Function Declarations ---
void get_client_ip(struct sockaddr_in client_adress);
void handle_signal(int sig);
int get_dataip_port(const char *buffer, ClientContext *client_context); // Pass ClientContext
void format_file_metadata(const char *filename, char *output, size_t output_size);
int setup_active_connection(ClientContext *client_context); // Pass ClientContext
void send_response(int client_fd, FTPStatusCode status_code, const char *message);
void handle_client(int client_fd, struct sockaddr_in client_adress);                  // New client handling function
void handle_user_command(int client_fd, char *buffer, ClientContext *client_context); // Command handlers
void handle_pass_command(int client_fd, char *buffer, ClientContext *client_context);
void handle_port_command(int client_fd, char *buffer, ClientContext *client_context);
void handle_list_command(int client_fd, ClientContext *client_context);
void handle_retr_command(int client_fd, char *buffer, ClientContext *client_context);
void handle_quit_command(int client_fd);
void handle_syst_command(int client_fd);
void handle_feat_command(int client_fd);
void handle_type_command(int client_fd, char *buffer, ClientContext *client_context);

// --- Function Implementations ---

void get_client_ip(struct sockaddr_in client_adress)
{
   // This setup is to retrieve info of client
   inet_ntop(AF_INET, &(client_adress.sin_addr), CLIENT_IP, INET_ADDRSTRLEN);
   printf("Client connected %s:%d\n", CLIENT_IP, ntohs(client_adress.sin_port));
}

void handle_signal(int sig)
{
   printf("\nClosing the server....\n");
   if (close(server_fd) < 0)
   {
      perror("Error closing server socket in signal handler");
   }
   exit(0);
}

int get_dataip_port(const char *buffer, ClientContext *client_context)
{
   int h1, h2, h3, h4, p1, p2;
   if (sscanf(buffer + 5, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) == 6)
   { // Check sscanf return
      if (snprintf(client_context->client_data_ip, sizeof(client_context->client_data_ip), "%d.%d.%d.%d", h1, h2, h3, h4) < 0)
      {
         perror("Error formatting client data IP");
         return -1;
      }
      client_context->data_port = p1 * 256 + p2;
      return 0;
   }
   else
   {
      fprintf(stderr, "Error parsing PORT command: %s\n", buffer); // Log error
      send_response(client_fd, FTP_SYNTAX_ERROR, "Invalid PORT parameters\r\n");
      return -1;
   }
}

void format_file_metadata(const char *filename, char *output, size_t output_size)
{
   struct stat metadata;
   if (stat(filename, &metadata) < 0)
   {
      perror("Stat failed");
      snprintf(output, output_size, "Error getting metadata for %s\r\n", filename);
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
   if (strftime(mod_time, sizeof(mod_time), "%b %d %H:%M", localtime(&metadata.st_mtime)) == 0)
   {
      strncpy(mod_time, "Unknown time", sizeof(mod_time));
      mod_time[sizeof(mod_time) - 1] = '\0';
   }

   if (snprintf(output,
               output_size,
               "%s %ld %s %s %ld %s %s\r\n",
               permissions,
               (long)metadata.st_nlink,
               pw ? pw->pw_name : "unknown",
               gr ? gr->gr_name : "unknown",
               (long)metadata.st_size,
               mod_time,
               filename) < 0)
   {
      perror("Error formatting file metadata string");
   }
}

int setup_active_connection(ClientContext *client_context)
{
   struct sockaddr_in client_data_adress;
   client_data_adress.sin_family = AF_INET;
   client_data_adress.sin_port = htons(client_context->data_port);
   if (inet_pton(AF_INET, client_context->client_data_ip, &client_data_adress.sin_addr) <= 0)
   {
      perror("inet_pton failed for data IP");
      return -1;
   }
   int data_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (data_socket_fd < 0)
   {
      perror("Data socket creation failed");
      return -1;
   }
   if (connect(data_socket_fd, (struct sockaddr *)&client_data_adress, sizeof(client_data_adress)) < 0)
   {
      perror("Data connection failed");
      close(data_socket_fd);
      return -1;
   }
   else
   {
      return data_socket_fd;
   }
}

void send_response(int client_fd, FTPStatusCode status_code, const char *message)
{
   char response_buffer[1024]; // Local buffer for response
   if (snprintf(response_buffer, sizeof(response_buffer), "%d %s", status_code, message) < 0)
   {
      perror("Error formatting response");
      return; // Or handle error more explicitly
   }
   printf("Sent: %s", response_buffer); // printf adds newline, no need in message
   if (send(client_fd, response_buffer, strlen(response_buffer), 0) < 0)
   {
      perror("send failed"); // Error handling for send
   }
}

void handle_client(int client_fd, struct sockaddr_in client_adress)
{
   ClientContext client_context;
   memset(&client_context, 0, sizeof(ClientContext)); // Initialize context

   send_response(client_fd, FTP_NEW_CONNECT, "Welcome to the FTP Server.\r\n"); // Use enum

   char buffer[1024];

   while (1)
   {
      ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
      if (bytes_received > 0)
      {
         buffer[bytes_received] = '\0';
         printf("Received: %s", buffer); // printf adds newline

         if (strncmp(buffer, "USER ", 5) == 0)
         {
            handle_user_command(client_fd, buffer, &client_context);
         }
         else if (strncmp(buffer, "PASS ", 5) == 0)
         {
            handle_pass_command(client_fd, buffer, &client_context);
         }
         else if (strncmp(buffer, "QUIT", 4) == 0)
         {
            handle_quit_command(client_fd);
            break; // Exit client loop after QUIT
         }
         else if (strncmp(buffer, "PORT ", 5) == 0)
         {
            handle_port_command(client_fd, buffer, &client_context);
         }
         else if (strncmp(buffer, "LIST", 4) == 0)
         {
            handle_list_command(client_fd, &client_context);
         }
         else if (strncmp(buffer, "SYST", 4) == 0)
         {
            handle_syst_command(client_fd);
         }
         else if (strncmp(buffer, "FEAT", 4) == 0)
         {
            handle_feat_command(client_fd);
         }
         else if (strncmp(buffer, "TYPE ", 5) == 0)
         {
            handle_type_command(client_fd, buffer, &client_context);
         }
         else if (strncmp(buffer, "RETR ", 5) == 0)
         {
            handle_retr_command(client_fd, buffer, &client_context);
         }
         else
         {
            send_response(client_fd, FTP_UNKNOWN_COMMAND, "Unknown command.\r\n"); // More informative
         }
      }
      else if (bytes_received == 0)
      {
         printf("Client disconnected gracefully.\n");
         break; // Client closed connection
      }
      else
      {
         perror("recv failed"); // Handle recv error
         break;                 // Exit client loop on recv error
      }
   }
   if (close(client_fd) < 0)
   {
      perror("Error closing client socket");
   }
}

void handle_user_command(int client_fd, char *buffer, ClientContext *client_context)
{
   strncpy(client_context->username, buffer + 5, sizeof(client_context->username) - 1);
   client_context->username[strcspn(client_context->username, "\r\n")] = '\0';
   printf("Username: %s\n", client_context->username);
   send_response(client_fd, FTP_USER_OK_PASS_REQ, "Password required.\r\n"); // Use enum
}

void handle_pass_command(int client_fd, char *buffer, ClientContext *client_context)
{
   strncpy(client_context->password, buffer + 5, sizeof(client_context->password) - 1);
   client_context->password[strcspn(client_context->password, "\r\n")] = '\0';
   printf("Password provided.\n");

   if (strcmp(client_context->username, "anon") == 0)
   {
      client_context->authenticated = 1;
      send_response(client_fd, FTP_LOGIN_SUCESS, "Login successful for anonymous user.\r\n"); // Use enum
   }
   else if (strcmp(client_context->username, USER_DEFAULT) == 0 && strcmp(client_context->password, PASS_DEFAULT) == 0)
   {
      client_context->authenticated = 1;
      send_response(client_fd, FTP_LOGIN_SUCESS, "Login successful.\r\n"); // Use enum
   }
   else
   {
      send_response(client_fd, FTP_AUTH_ERROR, "Authentication failed.\r\n"); // Use enum
   }
}

void handle_port_command(int client_fd, char *buffer, ClientContext *client_context)
{
   if (get_dataip_port(buffer, client_context) == 0)
   {
      printf("Data connection parameters set: IP=%s, Port=%d\n", client_context->client_data_ip, client_context->data_port);
      send_response(client_fd, FTP_OK, "PORT command successful.\r\n"); // Use enum
   }
   // Error response is already handled in get_dataip_port
}

void handle_list_command(int client_fd, ClientContext *client_context)
{
   DIR *dirp = NULL;
   struct dirent *dp = NULL;

   if (!client_context->authenticated)
   {
      send_response(client_fd, FTP_AUTH_ERROR, "Authentication required for LIST.\r\n"); // Use enum
      return;
   }

   client_context->data_socket_fd = setup_active_connection(client_context);
   if (client_context->data_socket_fd < 0)
   {
      send_response(client_fd, FTP_DATA_CONNECTION_FAIL, "Failed to open data connection.\r\n"); // Use enum
      return;
   }
   send_response(client_fd, FTP_DATA_OPEN, "Opening data connection for directory listing.\r\n"); // Use enum

   if ((dirp = opendir(".")) == NULL)
   {
      close(client_context->data_socket_fd);
      send_response(client_fd, FTP_FILE_ERROR, "Could not open directory, permission denied.\r\n"); // Use enum
      return;
   }

   while ((dp = readdir(dirp)) != NULL)
   {
      if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
      {
         continue;
      }
      char entry[256];
      format_file_metadata(dp->d_name, entry, sizeof(entry));
      if (send(client_context->data_socket_fd, entry, strlen(entry), 0) < 0)
      {
         perror("Error sending directory entry");
         break; // Stop sending if there's an error
      }
   }

   if (closedir(dirp) < 0)
   {
      perror("Error closing directory");
   }
   if (close(client_context->data_socket_fd) < 0)
   {
      perror("Error closing data socket");
   }
   send_response(client_fd, FTP_TRANSFER_COMPLETE, "Directory listing completed.\r\n"); // Use enum
}

void handle_retr_command(int client_fd, char *buffer, ClientContext *client_context)
{
   char file_name[256];
   strncpy(file_name, buffer + 5, sizeof(file_name) - 1);
   file_name[strcspn(file_name, "\r\n")] = '\0';
   printf("Client requested file: %s\n", file_name);

   if (!client_context->authenticated)
   {
      send_response(client_fd, FTP_AUTH_ERROR, "Authentication required for RETR.\r\n");
      return;
   }

   FILE *file = fopen(file_name, client_context->transfer_mode == BIN ? "rb" : "r");
   if (!file)
   {
      send_response(client_fd, FTP_FILE_ERROR, "File not found or inaccessible.\r\n"); // Use enum
      return;                                                                          // Important: exit function if file not found
   }

   client_context->data_socket_fd = setup_active_connection(client_context);
   if (client_context->data_socket_fd < 0)
   {
      fclose(file);                                                                                                // Close file if data connection fails
      send_response(client_fd, FTP_DATA_CONNECTION_FAIL, "Failed to open data connection for file transfer.\r\n"); // Use enum
      return;
   }
   send_response(client_fd, FTP_DATA_OPEN, "Opening data connection for file transfer.\r\n"); // Use enum

   char file_buffer[1024];
   size_t bytes_read;
   while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
   {
      if (send(client_context->data_socket_fd, file_buffer, bytes_read, 0) < 0)
      {
         perror("Error sending file data");
         fclose(file);
         close(client_context->data_socket_fd);
         return; // Stop sending if error occurs
      }
   }

   if (fclose(file) < 0)
   {
      perror("Error closing file");
   }
   if (close(client_context->data_socket_fd) < 0)
   {
      perror("Error closing data socket");
   }
   send_response(client_fd, FTP_TRANSFER_COMPLETE, "File transfer completed successfully.\r\n"); // Use enum
}

void handle_quit_command(int client_fd)
{
   send_response(client_fd, FTP_GOODBYE, "Goodbye.\r\n"); // Use enum
                                                          // client socket will be closed in handle_client function after loop breaks
}

void handle_syst_command(int client_fd)
{
   send_response(client_fd, FTP_SYST, "UNIX Type: L8\r\n"); // Use enum
}

void handle_feat_command(int client_fd)
{
   send_response(client_fd, FTP_INFO, "No features implemented.\r\n"); // Use enum
}

void handle_type_command(int client_fd, char *buffer, ClientContext *client_context)
{
   if (strncmp(buffer + 5, "I", 1) == 0)
   {
      client_context->transfer_mode = BIN;
      send_response(client_fd, FTP_OK, "Type set to binary.\r\n"); // Use enum
   }
   else if (strncmp(buffer + 5, "A", 1) == 0)
   {
      client_context->transfer_mode = ASC;
      send_response(client_fd, FTP_OK, "Type set to ASCII.\r\n"); // Use enum
   }
   else
   {
      send_response(client_fd, FTP_UNKNOWN_COMMAND, "Unsupported transfer type.\r\n"); // Use enum
   }
}

int main(int argc, char *argv[])
{
   printf("FTP Server v2.2.1\n");
   if (argc != 2)
   {
      fprintf(stderr, "Usage: %s <port>\n", argv[0]);
      return EXIT_FAILURE;
   }

   signal(SIGINT, handle_signal);

   int PORT = atoi(argv[1]);
   if (PORT <= 0 || PORT > 65535)
   {
      fprintf(stderr, "Invalid port number: %d\n", PORT);
      return EXIT_FAILURE;
   }

   struct sockaddr_in server_adress;
   server_adress.sin_family = AF_INET;
   server_adress.sin_port = htons(PORT);
   server_adress.sin_addr.s_addr = INADDR_ANY;

   socklen_t sock_len = sizeof(server_adress);

   server_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (server_fd < 0)
   {
      perror("Socket creation failed");
      return EXIT_FAILURE;
   }
   else
   {
      printf("Socket created.\n");
   }

   if (bind(server_fd, (struct sockaddr *)&server_adress, sock_len) < 0)
   {
      perror("Socket binding failed");
      if (close(server_fd) < 0)
         perror("Error closing server socket after bind failure");
      return EXIT_FAILURE;
   }
   else
   {
      printf("Socket binded to port: %d\n", PORT);
   }

   if (listen(server_fd, 5) < 0)
   {
      perror("Listen failed");
      if (close(server_fd) < 0)
         perror("Error closing server socket after listen failure");
      return EXIT_FAILURE;
   }
   else
   {
      printf("Listening on port: %d\n", PORT);
   }

   while (1)
   {
      struct sockaddr_in client_adress;
      socklen_t client_len = sizeof(client_adress);
      client_fd = accept(server_fd, (struct sockaddr *)&client_adress, &client_len);

      if (client_fd < 0)
      {
         perror("Failed to accept incoming connection");
         continue; // Continue to accept other connections
      }
      else
      {
         get_client_ip(client_adress);
         handle_client(client_fd, client_adress); // Handle client in separate function
      }
   }
   // server_fd is closed in signal handler
   return 0;
}