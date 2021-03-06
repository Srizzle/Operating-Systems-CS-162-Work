#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;





int check_directory(char* temp_dir){
  struct stat temp_stat;
  stat(temp_dir, &temp_stat);
  return S_ISDIR(temp_stat.st_mode);
}

int check_file(char* temp_dir){
  struct stat temp_stat;
  stat(temp_dir, &temp_stat);
  return S_ISREG(temp_stat.st_mode);
}

int file_size(char* temp_dir){
  struct stat temp_stat;
  stat(temp_dir, &temp_stat);
  return (int)temp_stat.st_size;
}

int dir_contains_index(char* temp_dir){
  char new_file_dir[1000]; 
  strcpy(new_file_dir, temp_dir );
  strcat(new_file_dir, "/index.html");
  /*
  struct stat temp_stat;
  stat(new_file_dir, & temp_stat);
  return S_ISREG(temp_stat.st_size);
  */
  return check_file(new_file_dir);
}


void send_file_to_fd(int fd, char* temp_dir){


  int opened = open(temp_dir, O_RDONLY);

  char temp_buff[100000];

  while(read(opened, temp_buff, sizeof(temp_buff)) > 0){
    http_send_data(fd, temp_buff, sizeof(temp_buff) );
  }
  close(opened);
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {
  printf("handle \n");
  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);
  
  
  char previous_file_dir[1000];

  /*
  getcwd(previous_file_dir, sizeof(previous_file_dir));

  */
  strcat(previous_file_dir, "/");

  strcat(previous_file_dir, server_files_directory);

  

  
  if( server_files_directory[strlen(server_files_directory)-1] == '/'){
    previous_file_dir[strlen(previous_file_dir)-1] = '\0';
  }
  
  strcat(previous_file_dir, request -> path);

  if( previous_file_dir[strlen(previous_file_dir)-1] == '/'){
    previous_file_dir[strlen(previous_file_dir)-1] = '\0';
  }

  printf("%s \n", previous_file_dir);


  if(check_file(previous_file_dir)){
    http_start_response(fd, 200);

    printf("Check file \n");
    

    http_send_header(fd, "Content-Type", http_get_mime_type(previous_file_dir));

    

    int temp_size = file_size(previous_file_dir);
    char temp_size_char[1000];
    sprintf(temp_size_char, "%d", temp_size);


    http_send_header(fd, "Content-Length", temp_size_char);
    http_end_headers(fd);
    send_file_to_fd(fd, previous_file_dir);
    close(fd);
    
  }
  else if(check_directory(previous_file_dir)){
    http_start_response(fd, 200);
    printf("Directory \n");
    
    check_file("/");
    check_file("/home/");
    check_file(previous_file_dir);


    if(dir_contains_index(previous_file_dir)){
      
      printf("Contains index\n");
      strcat(previous_file_dir, "/index.html");

      http_send_header(fd, "Content-Type", http_get_mime_type(previous_file_dir));

    

      int temp_size = file_size(previous_file_dir);
      char temp_size_char[1000];
      sprintf(temp_size_char, "%d", temp_size);


      http_send_header(fd, "Content-Length", temp_size_char);
      http_end_headers(fd);
      send_file_to_fd(fd, previous_file_dir);
      close(fd);
    }
    else{
      
      printf("Else \n");


      http_send_header(fd, "Content-Type", "text/html");

      char output[10000];
      strcat(output, "<a href=\"../\">");
      strcat(output, previous_file_dir);
      strcat(output, "</a>\n");

      DIR *d; 
      struct dirent *dir; 
      d = opendir(previous_file_dir);
      if(d){


        while ((dir = readdir(d)) != NULL) {
        
        if(strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") != 0){
          strcat(output, "<a href=\"../\">");
          strcat(output, dir->d_name);
          strcat(output, "</a>\n");
      }
      }
      closedir(d);
      }

      http_end_headers(fd);

      http_send_string(fd,
      output);

      close(fd);
    }

    
  }
  else{
    printf("Fail \n");
    http_start_response(fd, 404);
    http_end_headers(fd);
    close(fd);
  }

}



struct socket_input{
  int stop; 
  int cfd;
  int fd; 
};


void forward_data1(struct socket_input *argp){
  struct socket_input *args = argp; 

  int source_fd = args -> cfd; 
  int target_fd = args -> fd; 


  char temp_buff[1000];
  int read_success; 
  while(1){

    if (args -> stop ==1){
      return;
    }

    read_success = read(source_fd, temp_buff, sizeof(temp_buff));
    if(read_success > 0){
      http_send_string(target_fd, temp_buff);
    }
    else if(read_success == 0){
      //do nothing; 
    }
    else{
      args -> stop = 1; 
    }
  } 

}

void forward_data2(struct socket_input *argp){
  struct socket_input *args = argp; 

  int source_fd = args -> fd; 
  int target_fd = args -> cfd; 


  char temp_buff[1000];
  int read_success; 
  while(1){

    if (args -> stop ==1){
      return;
    }

    read_success = read(source_fd, temp_buff, sizeof(temp_buff));
    if(read_success > 0){
      http_send_string(target_fd, temp_buff);
    }
    else if(read_success == 0){
      //do nothing;
    }
    else{
      args -> stop = 1; 
    }
  } 

}






/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
  
  struct socket_input temp_input; 
  temp_input.cfd = client_socket_fd; 
  temp_input.fd = fd; 
  temp_input.stop = 0;

  pthread_t t1;
  pthread_t t2;

  pthread_create(&t1, NULL, (void * (*)(void *))&forward_data1, &temp_input);
  pthread_create(&t2, NULL, (void * (*)(void *))&forward_data2, &temp_input);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);


}





void req_thread(void *(request_handler)(int)){
  while(1){
    
    int temp_socket_id = wq_pop(&work_queue );
    request_handler(temp_socket_id);
    close(temp_socket_id);
  }
}
void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */


  wq_init(&work_queue);

  for(int i =0; i<num_threads; i++){
    pthread_t temp_t;
    
    pthread_create(&temp_t, NULL, (void * (*)(void *))&req_thread, request_handler);
  }
 

}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    if (num_threads ==0 ){
    request_handler(client_socket_number);
    close(client_socket_number);
    }else{
    wq_push(&work_queue, client_socket_number); //added new line
    //request_handler(client_socket_number);
    //close(client_socket_number);
}
    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;




  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }
  //chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
