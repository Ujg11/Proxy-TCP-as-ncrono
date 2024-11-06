//Alumne: Oriol Jiménez Garrich

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h> /* atoi(): ascii to integer function */
#include <sys/select.h>

#define BACKLOG     10  //Màxim de connexions que es poden tenir a la cua (es passa al listen)
#define MAX_LEN	    1024 //mida del buffer de lectura

// Global variable for destination server
struct sockaddr_in remoteaddr; //Adrecha del servidor al cual enviarem les dades

int handle(int newsock)
{
	fd_set readsocks; //Estructura per guardar els fds
	int remote_sock, maxsock, nbytes;
	char buffer[MAX_LEN]; //Buffer per les dades llegides
   
   
	/* 1- Create the remote socket */
	remote_sock = socket(AF_INET, SOCK_STREAM, 0);//creem un socket TCP (SOCK_STREAM)
	if (remote_sock == -1)
	{
		perror("remote socket() function failed");
		return (1);
	}
	/* 2- init connection to remote server (syn, syn+ack, ack)*/ 
	if (connect(remote_sock, (struct sockaddr*)&remoteaddr, sizeof(remoteaddr)) == -1) //Ens conectem amb el servidor remot
	{
		perror("remote connect() function failed");
		close(remote_sock);
		return (1);	
	}
	/* 3- Set up select parameters:  maxsock */
	if (newsock > remote_sock) //Fem que maxsock sigui el més gran per després passar-li al select
		maxsock = newsock;
	else
		maxsock = remote_sock;

	while(1)
	{ 
		/* Actions to be executed inside this loop:
		   1- Reconfigure select parameters: fd_set, remote_sock, newsock 
		   2- Select which socket is ready to be read
		   3- Read the ready socket and 
			  a- write to the pairing socket
			  b- Manage events as closing connection
		*/
		FD_ZERO(&readsocks);//Inicialitza el conjunt de descriptors de fitxer a 0
		FD_SET(newsock, &readsocks); //Afegeix newsock(el client) al conjunt de descriptors
		FD_SET(remote_sock, &readsocks); //Afegeix remote_sock (el servidor) al conjunt de descriptors
		
		/* El programa espera a que algun dels sockets estigui llest per llegir */
		if (select(maxsock + 1, &readsocks, NULL, NULL, NULL) == -1)
		{
			perror("select() function failed");
			break ;
		}
		if (FD_ISSET(newsock, &readsocks)) //Si hi ha dades del client
		{
			if ((nbytes = recv(newsock, buffer, MAX_LEN, 0)) <= 0) //llegim les dades del newsocket(client)
			{
				if (nbytes == 0)//si dona 0 vol dir que la connexió ha estat tancada
					printf("Client closed\n");
				else
					perror("recv() function in client failed");
				break ;
			}
			buffer[nbytes] = '\0';//hem de posar el \0 al final per indicar el fi de les dades transmeses
			if (send(remote_sock, buffer, nbytes, 0) == -1)
			{
				perror("send() function to remote server failed");
				break ;
			}
		}
		if (FD_ISSET(remote_sock, &readsocks)) //Si hi ha dades del servidor
		{
			if ((nbytes = recv(remote_sock, buffer, MAX_LEN, 0)) <= 0) //llegim les dades del remote_sock(servidor)
			{
				if (nbytes == 0)//si dona 0 vol dir que la connexió ha estat tancada
					printf("Destination server closed\n");
				else
					perror("recv() function in server failed");
				break ;
			}
			buffer[nbytes] = '\0';
			if (send(newsock, buffer, nbytes, 0) == -1)
			{
				perror("send() function to client failed");
				break ;
			}
		}
	}
	close(remote_sock);
	close(newsock);
	return (0);
}


/* Arguments:
   argv[0] = program name
   argv[1] = destination server IP address in dot-decimal char "xxx.xxx.xxx.xxx"
   argv[2] = destination server service port as char "xx"
   argv[3] = listen port of TCP Proxy server as char "yy"
   argc has to have a value of 4
*/
int main(int argc, char *argv[])
{
	int		sock;//socket descriptor we listen on
	struct sockaddr_in	servaddr;//servaddr: tcpproxy address. The socket address we listen on
	//struct sigaction	sa;
	int		reuseaddr = 1; /* True */
	char	*destination_ip = argv[1];
	char	*destination_port = argv[2];
	char	*proxy_port = argv[3];

	if(argc<4) {
	   printf("usage : %s destination-server detination-port listen-port\n", argv[0]);
	   return 1;
	}   
	/* Set up the signal handler */
	signal(SIGCHLD, SIG_IGN);
	/* Fill the "global" variable remoteaddr with the destination-server address and destination port */
	remoteaddr.sin_family = AF_INET; //Utilitzem IPv4
	remoteaddr.sin_port = htons(atoi(destination_port)); //Indiquem el port
	remoteaddr.sin_addr.s_addr = inet_addr(destination_ip); //Indiquem la IP del servidor

	/* Create the Listen TCP socket (socket d'escolta) */
	sock = socket(AF_INET, SOCK_STREAM, 0);//creem el socket_fd
	if (sock == -1)
	{
		perror("socket() function failed");
		return (1);
	}
	/* Permetem que la adreça es reutilitzi evitant problemes al intentar reconectar-se amb la mateixa adreça */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
		perror("setsockopt() function failed");
		close(sock);// Hem de tancar el sock abans d'acabar el programa per alliberar recursos
		return 1;
	}

	/* Fill in our server address (tcpproxy address) & bind listen socket to it & Listen*/
	servaddr.sin_family = AF_INET; //Utilitzem IPv4
	servaddr.sin_port = htons(atoi(proxy_port)); //Indiquem el port 
	servaddr.sin_addr.s_addr = INADDR_ANY; //Indiquem que el servidor acceptarà conexions en totes les IP locals
	/* Fem el bind al sock d'entrada de peticions */
	if (bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
	{
		perror("bind() function failed");
		close(sock);
		return (1);
	}

	/* Main loop */
	while (1) {
		struct sockaddr_in their_addr; //Adreça del client (connexió entrant)
		socklen_t size = sizeof(struct sockaddr_in);
		int newsock;   // newsock: the socket of the client connected to us
		int pid;       // pid: the var to hold the pid value returned by fork 
 
		// Accept incoming client connections checking errors
		//El programa es queda aqui esperant fins que reb una connexió
		newsock = accept(sock, (struct sockaddr *)&their_addr, &size);
		if (newsock == -1)
		{
			perror("accept() function failed");
			continue ;
		}
		// fork process
		pid = fork(); //Fem el fork per fer el programa asíncron
		if (pid == 0) /* Estem al procés fill */
		{
			close(sock);//ja no necessitem estar escoltant i tanquem el socket
			handle(newsock); //Gestionem la comunicació
			close(newsock); //un cop acabada la connexió tanquem el socket
			return 0;
		}
		else/* Estem al procés pare */
		{
			if (pid == -1) //Hi ha un error al fork()
			{
				perror("fork");
				return 1;
			}
			else
			{
				printf("Got a connection from %s on port %d\n", inet_ntoa(their_addr.sin_addr), htons(their_addr.sin_port));
				printf("Client served by child process %d\n\n", pid);
				close(newsock);//Tanquem el socket del client ja que el gestiona el fill
			}
		}
	}
	close(sock);
	return 0;
}
