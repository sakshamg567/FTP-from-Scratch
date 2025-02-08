## FTP Server Project

### Overview
This project implements a simple FTP (File Transfer Protocol) server in C. The server supports basic FTP commands for file transfer, directory listing, and user authentication. It is designed to provide a foundation for understanding the FTP protocol and socket programming.

### Features
#### Implemented Features
- **User Authentication**:
  - Supports both default user credentials (`USER` and `PASS`) and anonymous login (`anon`).
  - Responds with appropriate error messages for invalid credentials.
  
- **Directory Listing**:
  - Handles the `LIST` command to send formatted directory listings over the data connection.
  - Displays file metadata such as permissions, size, modification time, and filename.
  
- **File Transfer**:
  - Implements the `RETR` (retrieve/download) command for downloading files from the server.
  - Implements the `STOR` (store/upload) command for uploading files to the server.
  - Supports both binary (`TYPE I`) and ASCII (`TYPE A`) transfer modes.
  
- **Active Mode**:
  - Handles the `PORT` command to establish data connections in active mode.
  
- **System Information**:
  - Responds to the `SYST` command with system type information (e.g., `UNIX Type: L8`).
  
- **Graceful Shutdown**:
  - Handles `SIGINT` signals to close the server gracefully.
  
- **Error Handling**:
  - Provides meaningful error responses for unsupported commands, syntax errors, and permission issues.
  
#### Future Features (Planned)
- **Passive Mode**:
  - Implement the `PASV` command to allow clients to connect to the server for data transfers.
  
- **Concurrency**:
  - Support multiple simultaneous client connections using threads or multiplexing (e.g., `select()` or `poll()`).
  
- **Security Enhancements**:
  - Add support for FTPS (FTP over SSL/TLS) to secure data transfers.
  
- **Advanced Commands**:
  - Implement additional FTP commands such as `CWD` (change directory), `PWD` (print working directory), `DELE` (delete file), `MKD` (make directory), and `RMD` (remove directory).
  
- **Logging**:
  - Add logging functionality to record server activity for debugging and auditing purposes.
  
### Technical Details
#### Supported FTP Commands
- `USER <username>`: Authenticate the user.
- `PASS <password>`: Provide the password for authentication.
- `QUIT`: Terminate the session.
- `PORT h1,h2,h3,h4,p1,p2`: Specify the client's IP and port for active mode.
- `LIST`: Retrieve a directory listing.
- `RETR <filename>`: Download a file from the server.
- `STOR <filename>`: Upload a file to the server.
- `SYST`: Retrieve system information.
- `TYPE <type>`: Set the transfer mode (`I` for binary, `A` for ASCII).

#### Implementation Details
- **Socket Programming**:
  - Uses `socket()`, `bind()`, `listen()`, and `accept()` for the control connection.
  - Establishes a separate data connection for file transfers and directory listings.
  
- **File Operations**:
  - Uses standard file I/O functions (`fopen()`, `fread()`, `fwrite()`) for file handling.
  
- **Error Handling**:
  - Validates inputs and responds with appropriate FTP status codes (e.g., `500`, `550`).
  
- **Platform**:
  - Designed for Unix-like systems (Linux, macOS). May require adjustments for Windows compatibility.
  
### Installation and Usage
#### Prerequisites
- A Unix-like operating system (Linux or macOS).
- GCC compiler installed.
- Basic knowledge of FTP and socket programming.

#### Build Instructions
Clone the repository:
```bash
git clone https://github.com/your-repo/ftp-server.git
cd ftp-server
```

Compile the server:
```bash
gcc -o ftp_server ftp_server.c
```

Run the server:
```bash
./ftp_server <port>
```

#### Testing the Server
Use an FTP client like `ftp`, `lftp`, or `FileZilla` to connect to the server:
```bash
ftp localhost <port>
```

Example commands:
```
USER user
PASS pass
LIST
RETR test.txt
STOR newfile.txt
QUIT
```

### Screenshots
#### Server Startup
_(Placeholder for images)_

#### Client Interaction
_(Placeholder for images)_

#### Directory Listing
_(Placeholder for images)_

### Future Work
The following features are planned for future releases:
- **Passive Mode**:
  - Allow clients behind firewalls or NATs to initiate data connections.
  
- **Concurrency**:
  - Support multiple clients simultaneously using threads or non-blocking I/O.
  
- **Security**:
  - Implement FTPS (FTP over SSL/TLS) for encrypted communication.
  
- **Advanced Commands**:
  - Add support for additional FTP commands like `CWD`, `PWD`, `DELE`, `MKD`, and `RMD`.
  
- **Logging**:
  - Log server activity to a file for monitoring and debugging.
