#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h> /* the port number of web server */
#define BACKLOG 10 /* pending connections queue size */
#define MAXBYTES 1000

int server_sockfd, new_sockfd; // the server socket file description

void report_error_and_exit(const char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

// handle request from client
void handle_request(int client_sockfd, const char* buf);

void handle_sigint(int sig)
{
    if (sig == SIGINT)
    {
        printf("shut down server.\n");
        close(new_sockfd);
        close(server_sockfd);
        exit(0);
    }
}

int main(int argc, char* argv[])
{
	struct sockaddr_in my_addr;
	struct sockaddr_in user_addr;
    if (argc != 2)
    {
        printf("Error: please specify the port number.\n");
        report_error_and_exit("Usage: ./webserver [portno]");
    }
    
    signal(SIGINT, handle_sigint); // if catch ctrl-c signal, shut down the server
    
    int portno;
    portno = atoi(argv[1]);
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sockfd == -1)
		report_error_and_exit("Error creating sockets");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(portno);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // store ip address of server
	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

	if (bind(server_sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1)
		report_error_and_exit("Error binding sockets");

	if (listen(server_sockfd, BACKLOG) == -1)
		report_error_and_exit("Error listening on sockets");

	while (1)
	{
		int addr_size;
		new_sockfd = accept(server_sockfd, (struct sockaddr*) &user_addr, (socklen_t *) &addr_size);
		if (new_sockfd == -1)
		{
			perror("failed to accept connections");
			continue;
		}

		int nread;
		char buf[MAXBYTES];
		/* part 1: log the HTTP request */
		nread = read(new_sockfd, buf, MAXBYTES);
		if (nread == -1)
			report_error_and_exit("Error reading from client.");
		if (write(STDOUT_FILENO, buf, nread) == -1)
			report_error_and_exit("Error printing HTTP request.");

		handle_request(new_sockfd, buf);
		// printf("%s\n",response);
		// free(response);
        close(new_sockfd);

	}

}

// compare two strings in case insenstive mode
// return 0 if two strings are equal
int my_strcmp(const char* x, const char* y)
{

    size_t i = 0;
    size_t j = 0;
    char filename[256];
    while (i < strlen(x))
    {
        if (x[i] == '%' && x[i + 1] == '2' && x[i + 2] == '0')
        {
            filename[j] = ' ';
            i += 3;
        }
        else
        {
            filename[j] = x[i];
            ++i;
        }
        ++j;
    }
    filename[j] = '\0';
    // printf("want to filename: %s", filename);
    
    if (strlen(filename) != strlen(y))
        return 1;
    
    for (j = 0; j != strlen(filename); ++j)
        if (tolower(filename[j]) != tolower(y[j]))
            return 1;

	return 0;
}

char* get_file_type(const char* filename)
{
	size_t i;
	char extension[10];
	for (i = 0; i != strlen(filename); ++i)
		extension[i] = tolower(extension[i]);
	
	for (i = 0; i != strlen(filename); ++i)
	{
		if (filename[i] == '.')
		{
			strcpy(extension, filename + i + 1);
			break;
		}
	}
	char* file_type = (char*) malloc(64 * sizeof(char));
	if (i == strlen(filename))
		strcpy(file_type, "application/octet-stream");
    // printf("file type: %s\n", extension);
	else if (strcmp(extension, "html") == 0 || strcmp(extension, "htm") == 0)
		strcpy(file_type, "text/html");
	else if (strcmp(extension, "txt") == 0)
		strcpy(file_type, "text/plain");
	else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0|| strcmp(extension, "png") == 0 || strcmp(extension, "gif") == 0)
		sprintf(file_type, "image/%s", extension);
	else
		sprintf(file_type, "text/%s", extension);

	return file_type;

}

// handle request from client
void handle_request(int client_sockfd, const char* buf)
{
	size_t i = 0;
	while (1)
	{
		if (buf[i] == '\r' && buf[i + 1] == '\n')
			break;
		++i;
	}
	char line[MAXBYTES];
	memcpy(line, buf, i);
	line[i] = '\0';
	// retrieve the first line

	char response[MAXBYTES];
	if (strncmp(line, "GET", 3) != 0)
	{
		strcpy(response, "HTTP/1.1 400 Bad Request\r\n\r\n");
		if (write(client_sockfd, response, strlen(response)) == -1)
			report_error_and_exit("Error writing to the client socket.");
		return;
	} // our server can only handle get request

	char filename[256];
	strcpy(filename, line + 5);
    // skip "GET \"
	for (i = strlen(filename) - 1; filename[i] != ' '; --i)
		;
	filename[i] = '\0';
    // printf("request file: %s\n", filename);
	// retrieve the filename

	DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d == NULL)
    	report_error_and_exit("Error opening the current working directory.");

    // scan files in the current directory
    int flag = 0;
    //printf("Start searching the directory.\n");
    while ((dir = readdir(d)) != NULL)
    {
        //fprintf(stderr, "filename is: %s\n",filename);
        //fprintf(stderr,"search file: %s\n", dir->d_name);
        //printf("in the loop\n");
    	if (my_strcmp(filename, dir->d_name) == 0) // matched file name !!
        {
        	//fprintf(stderr, "search file: %s\n", filename);
            strcpy(filename, dir->d_name);
        	// search the actual file name on server side
        	struct stat file_st;
        	if (stat(filename, &file_st) == -1)
        		report_error_and_exit("Error getting file info.");
        	// get current time and date
        	time_t rawtime;
        	time ( &rawtime );
			struct tm * timeinfo;
			timeinfo = localtime(&rawtime);
			// check file type from extensions
            char date[50];
            strcpy(date, asctime(timeinfo));
            date[strlen(date)-1] = '\0';
            char last_modified[50];
            strcpy(last_modified, ctime(&file_st.st_mtime));
            last_modified[strlen(last_modified)-1] = '\0';
			char* file_type;
			file_type = get_file_type(filename);
			sprintf(response, "HTTP/1.1 200 OK\r\nConnection: close\r\nDate: %s\r\nLast-Modified: %s\r\nContent-Length: %lld\r\nContent-Type: %s\r\n\r\n",
				date, last_modified, (long long)file_st.st_size, file_type);

			flag = 1; 
			free(file_type);
			break;

        }
    }
    if (! flag)
    {
        // printf("File not found.\n");
    	strcpy(response, "HTTP/1.1 404 Not Found\r\n\r\n");
		if (write(client_sockfd, response, strlen(response)) == -1)
			report_error_and_exit("Error writing to the client socket.");
		return;
    }
    closedir(d);
    // finish the http response

    if (write(client_sockfd, response, strlen(response)) == -1)
			report_error_and_exit("Error writing to the client socket.");
	// send the header to client

	int fd = open(filename, O_RDONLY);
	if (fd == -1)
		report_error_and_exit("Error opening the file.");

	int nread;
	char temp[MAXBYTES];
	while(1)
	{
		nread = read(fd, temp, MAXBYTES);
		if (nread < 0 )
			report_error_and_exit("Error reading from socket.");
		if (nread > 0)
			if (write(client_sockfd, temp, nread) == -1)
				report_error_and_exit("Error writing to client socket.");
		if (nread == 0)
			break;
	}
	close(fd);
    return;

}
