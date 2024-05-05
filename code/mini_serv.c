#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

// On definit une structure de donnees pour chaque client
typedef struct s_client {
    int     id;
    char    msg[100000];
}   t_client;

// Definition des variables, tableau de structures max de 1024 (max des fd disponibles)
// fd_set est un type de données, un fd_set est juste un ensemble de fd.
// int maxfd=0, est créé pour surveiller le nombre de fd ouverts (++ lorsqu'un client se connecte)
// int gid=0, définit une variable pour attribuer un code Id à chaque client qui se connecte.
t_client    clients[1024];
fd_set      read_set, write_set, current;
int         maxfd = 0, gid = 0;
char        send_buffer[120000], recv_buffer[120000];

void    err(char  *msg) {
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error", 11);
    write(2, "\n", 1);
    exit(1);
}

/*
La fonction itère sur tous les fd ouverts (on ne lui donne en paramètre que le fd except),
vérifie sur le fd est en état d'écrire et si on est sur un fd different de celui interdit,
puis verifie si on peut bien envoyer le message à écrire au fd, avec le buffer d'envoi,
retourne -1 si le send a échoué.
FD_ISSET permet de verifier que le fd fait bien partie du set d'ecriture autorisé, on lui
passe en argument le fd concerne et un pointeur vers le fd_set (ici c'est write_set).
Si le fd fait bien partie du fd_set, la macro renvoie un int != 0, sinon 0.
Send() est une fonction pour envoyer des data à travers un socket connecté, la signature est :

    ssize_t send(int sockfd, const void *buf, size_t len, int flags);

Si la fonction send echoue on appelle la fonction err(NULL) => écrira "fatal error"
*/
void    send_to_all(int except) {

    for (int fd = 0; fd <= maxfd; fd++) {
        if  (FD_ISSET(fd, &write_set) && fd != except)
            if (send(fd, send_buffer, strlen(send_buffer), 0) == -1)
                err(NULL); }
}

int     main(int ac, char **av) {

    if (ac != 2)
        err("Wrong number of arguments");

    struct sockaddr_in  serveraddr; // structure pour stocker l'adresse du serveur (port et adress de socket)
    socklen_t           len; // variable qui stocke la longueur des adresses des sockets utilisés
    int serverfd = socket(AF_INET, SOCK_STREAM, 0); // Création du socket du serveur qui va ecouter les connexions
    if (serverfd == -1) // si la fonction socket échoue elle retourne -1
        err(NULL);
    maxfd = serverfd; // maxfd est initialisé sur l'adresse du socket serveur

    FD_ZERO(&current); // on met tous les fd de l'ensemble du set à vide, on "nettoie"
    FD_SET(serverfd, &current); // on met le socket du serveur "serverfd" dans le fd_set current
    bzero(clients, sizeof(clients)); // on met toutes les données des clients à 0 (nettoyage)
    bzero(&serveraddr, sizeof(serveraddr)); // idem on nettoie la structure serveraddr

    serveraddr.sin_family = AF_INET; // specifie que la famille d'adresses utilisées est IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // spécifie l'adresse IP du serveur.
    // INADDR_ANY signifie que le serveur écoutera sur toutes les interfaces réseau de la machine.
    // htonl() est utilisée pour convertir l'adresse IPv4 de l'hôte au format réseau.
    serveraddr.sin_port = htons(atoi(av[1])); // spécifie le port sur lequel le serveur ecoutera
    // les connexions entrantes. av[1] est le port fourni en argument. htons convertit le numero du port
    // au format réseau.

    // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) On utilise bind() pour lier
    // une adresse de serveur avec un websocket.

    if (bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1)
        err(NULL);


// dans la boucle on met sur "ecoute" les fd avec select()
// int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
// nfds est le nombre max de fd ouverts, ensuite les set de fd de lecture et d'ecriture, enfin deux
// options de select qui concernent les fd exceptions et le timeout possible.
// select() surveille les fd ouverts pour lancer ensuite les operations de lecture/ecriture
    while (1) {

        read_set = write_set = current;
        if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) continue;

        // on verifie sur chaque fd ouvert si une operation de lecture est en attente
        for (int fd = 0; fd <= maxfd; fd++) {

            // on verifie si le fd est autorisé en lecture
            if (FD_ISSET(fd, &read_set))
            {
                // si on est sur le fd du serveur, signifie qu'il s'agit d'une demande de connexion
                // on utilise donc la fonction accept() pour
                if (fd == serverfd)
                {
                    int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);
                    if (clientfd == -1) continue;
                    if (clientfd > maxfd)
                        maxfd = clientfd;
                    clients[clientfd].id = gid++;
                    FD_SET(clientfd, &current);
                    sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd);
                    break;
                }
                // si on est pas sur le fd du serveur, c'est un client qui veut ecrire
                else
                {
                    int ret = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
                    if (ret <= 0)
                    {
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd);
                        FD_CLR(fd, &current);
                        close(fd);
                        break;
                    }
                    else
                    {
                        for (int i = 0, j = strlen(clients[fd].msg); i < ret; i++, j++)
                        {
                            clients[fd].msg[j] = recv_buffer[i];
                            if (clients[fd].msg[j] == '\n')
                            {
                                clients[fd].msg[j] = '\0';
                                sprintf(send_buffer, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                                send_to_all(fd);
                                bzero(clients[fd].msg, strlen(clients[fd].msg));
                                j = -1;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    return (0);
}
