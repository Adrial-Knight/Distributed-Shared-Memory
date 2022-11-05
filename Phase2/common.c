#include "common_impl.h"

/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */

int socket_and_connect(char *hostname, char *port) {
	int sock_fd = -1;
	// Création de la socket
	if (-1 == (sock_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("Socket");
		exit(EXIT_FAILURE);
	}
	struct addrinfo hints, *res, *tmp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
	int error;
	error = getaddrinfo(hostname, port, &hints, &res);
	if (error) {
		errx(1, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}
	tmp = res;
	while (tmp != NULL) {
		if (tmp->ai_addr->sa_family == AF_INET) {
			fflush(stdout);
			if (-1 == connect(sock_fd, tmp->ai_addr, tmp->ai_addrlen)) {
				return -1;
			}
			return sock_fd;
		}
		tmp = tmp->ai_next;
	}
	return -1;
}

int listening_socket(int num_procs) {
  int listening_sock = -1;
  //socklen_t sock_len = sizeof(struct sockaddr_in);

  if ((listening_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Error while creating socket");
    return -1;
  }

  int yes = 1;
	if (-1 == setsockopt(listening_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

    struct addrinfo indices;
    memset(&indices, 0, sizeof(struct addrinfo));
    indices.ai_family = AF_INET;
    indices.ai_socktype = SOCK_STREAM;
    indices.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
    struct addrinfo *res, *tmp;

    int err = 0;
    if (0 != (err = getaddrinfo(NULL, "0", &indices, &res))) {
    errx(1, "%s", gai_strerror(err));
    }

    tmp = res;

    while (tmp != NULL) {
    if (tmp->ai_family == AF_INET) {
        if (-1 == bind(listening_sock, tmp->ai_addr, tmp->ai_addrlen)) {
            perror("Binding");
        }
        if (-1 == listen(listening_sock, num_procs)) {
            perror("Listen");
        }
        return listening_sock;
    }
    tmp = tmp->ai_next;
}


  return listening_sock;

}

int get_associated_port(int sock_fd){
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	if (getsockname(sock_fd, (struct sockaddr *)&sin, &len) == -1){
		perror("getsockname");
	}

	return ntohs(sin.sin_port);
}

int accept_client(int listening_sock){
    // accept new connection and retrieve new socket file descriptor
    struct sockaddr_in client_addr;
    memset(&client_addr, '\0', sizeof(struct sockaddr_in));
    int sockfd = -1;
    int socklen = sizeof(struct sockaddr_in);

    if((sockfd = accept(listening_sock, (struct sockaddr *)&client_addr, (socklen_t *)&socklen)) == -1){
        perror("Error while accepting");
    }

    return sockfd;

}



void write_int_size(int fd, void *ptr) {
	int ret = 0, offset = 0;
	while (offset != sizeof(int)) {
		ret = write(fd, ptr + offset, sizeof(int) - offset);
		if (-1 == ret)
			perror("Writing size");
		offset += ret;
	}
}

int read_int_size(int fd) {
	int read_value = 0;
	int ret = 0, offset = 0;
	while (offset != sizeof(int)) {
		ret = read(fd, (void *)&read_value + offset, sizeof(int) - offset);
		if (-1 == ret && errno == EINTR){
			sleep(1);
			continue;
		}
		else if(-1 == ret){
			perror("Reading size");
		}
		if (0 == ret) {
			printf("Should close connection, read 0 bytes\n");
			close(fd);
			return -1;
		}
		offset += ret;
	}
	return read_value;
}

int read_from_socket(int fd, void *buf, int size) {
	int ret = 0;
	int offset = 0;
	while (offset != size) {
		ret = read(fd, buf + offset, size - offset);
		if (-1 == ret && errno == EINTR) {
			sleep(1);
			continue;
		}
		if (0 == ret) {
			//close(fd);
			return -1;
			break;
		}
		offset += ret;
	}
	return offset;
}

int write_in_socket(int fd, void *buf, int size) {
	int ret = 0, offset = 0;
	while (offset != size) {
		if (-1 == (ret = write(fd, buf + offset, size - offset))) {
			perror("Writing from client socket");
			return -1;
		}
		offset += ret;
	}
	return offset;
}

pthread_mutex_t my_fprintf_mutex = PTHREAD_MUTEX_INITIALIZER;

void my_fprintf(FILE *stream, color_t color, effect_t effect, char* format, ...){
    va_list args;
    va_start(args, format);

	pthread_mutex_lock(&my_fprintf_mutex);
    fprintf(stream, "\033[%i;%im", effect, color);
    vfprintf(stream, format, args);
    fprintf(stream, "\033[0m\n");
	pthread_mutex_unlock(&my_fprintf_mutex);

    va_end(args);
    fflush(stream);
}
