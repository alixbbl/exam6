#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
	int id;
	char *msg[100000];
}	t_client;

t_client	clients[1024];
int			maxfd, gid;
fd_set		read_set, write_set, current;
char		send_buffer[120000], recv_buffer[120000];

void	err(char *msg) {

	if (msg)
		write(2, msg, strlen(msg));
	else
		write(2, "Fatal error", 11);
	write(2, "\n", 1);
	exit(1);
}

// la focntion est faite pour ecrire depuis un fd, dont on verifie qu' il appartient bien
// aux fd du set d'ecriture
void	send_to_all(int except) {

	for (int fd = 0; fd <= maxfd; fd++) {
		if (FD_ISSET(fd, &write_set) && fd != except)
			if (send(fd, send_buffer, strlen(send_buffer), 0) == -1)
				err(NULL);
	}
}

int main(int ac, char **ag) {

	if (ac != 2)
		err("Wrong number of argument");

	struct sockaddr_in 	serveraddr;
	socklen_t			len;
	int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd == -1)
		err(NULL);
	maxfd = serverfd;

	FD_ZERO(&current);
	FD_ISSET(serverfd, &current);
	bzero(clients, sizeof(clients));
	bzero(&serveraddr, sizeof(serveraddr));

	if (bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1) err(NULL);

	while (1) {

		read_set = write_set = current;
		if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) continue;

		for (int fd = 0; fd <= maxfd; fd++) {

			if (FD_ISSET(fd, &read_set))
			{
				if (fd == serverfd)
				{
					int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);
					if (clientfd == -1) continue;
					if (clientfd > maxfd)
						maxfd = clientfd;
					clients[clientfd].id = gid++;
					FD_SET(clientfd, &current);
				}
				else
				{

				}
			}
		}
	}
	return (0);
}
