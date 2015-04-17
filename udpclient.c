/* udpclient.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <malloc.h>

/*
Command Line Arguments
Port No :argv[1]
IP address :argv[2]
 */
int t=0;
int node_id = 2;
char port[1024]="2000";
char host_name[1024] = "localhost";
int *sock;
int n,e;  //number of nodes
int** data;
struct sockaddr_in *server_addr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
void *sender_hello(void *arg){
    int i;
    char str[1024];
    char send_str[1024];
    while (1) {
        for(i=0;i<e;i++){
            if(data[i][0]==node_id){
                sprintf(str, "%d", node_id);
                strcat(send_str,"HELLO-");
                strcat(send_str,str);
                sendto(sock[node_id],send_str, strlen(send_str), 0,
                    (struct sockaddr *) &server_addr[data[i][1]], sizeof (struct sockaddr));
                sleep(1);
                strcpy(str,"");
                strcpy(send_str,"");
            }
            if(data[i][1]==node_id){
                sprintf(str, "%d", node_id);
                strcat(send_str,"HELLO-");
                strcat(send_str,str);
                sendto(sock[node_id],send_str, strlen(send_str), 0,
                    (struct sockaddr *) &server_addr[data[i][0]], sizeof (struct sockaddr));
                sleep(1);
                strcpy(str,"");
                strcpy(send_str,"");
            }
        }
    }
}

void *recver_fun(void *arg){
    int sock_recv;
    int addr_len, bytes_read;
    char recv_data[1024];
    struct sockaddr_in server_addr, client_addr;

    if ((sock_recv = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port)+node_id);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero), 8);

    if (bind(sock_recv, (struct sockaddr *) &server_addr,
            sizeof (struct sockaddr)) == -1) {
        perror("Bind");
        exit(1);
    }

    addr_len = sizeof (struct sockaddr);

    printf("\nUDPServer Waiting for client on port %d\n", atoi(port)+node_id);
    fflush(stdout);

    while (1) {
        bytes_read = recvfrom(sock_recv, recv_data, 1024, 0,
                (struct sockaddr *) &client_addr, &addr_len);


        recv_data[bytes_read] = '\0';

        //printf(/"\n(%s , %d) said : ", inet_ntoa(client_addr.sin_addr),
        //        ntohs(client_addr.sin_port));
        printf("DATA:-%s\n", recv_data);
        fflush(stdout);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    pthread_t sender;
    pthread_t recver;
    FILE *fp;
    fp = fopen("input","r");
    fscanf(fp,"%d %d",&n,&e);
    //printf("%d %d",n,e);
    int j,k;
    //printf("%d",argc);
    for(j=1;j<argc;j++){
        if(strcmp(argv[j],"-i")){
            node_id = atoi(argv[2]);
            printf("Node id is: %d\n",node_id);
            j++;
        }
    }
    
    data = (int **)malloc(e*sizeof(int*));
    for(j=0;j<e;j++){
        *(data+j) = (int *) malloc(4*sizeof(int));
        for(k=0;k<4;k++){
            fscanf(fp,"%d",&data[j][k]);
        }

    }
    printf("DATA[0] %d %d %d %d",data[0][0],data[0][1],data[0][2],data[0][3]);
    struct hostent *host;
    host = (struct hostent *) gethostbyname(host_name);
    char send_data[1024];
    int i=0;
    sock = (int *)malloc(n*sizeof(int));
    server_addr = (struct sockaddr_in*)malloc(n*sizeof(struct sockaddr_in));
    for(i=0;i<n;i++){
        if ((sock[i] = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }

        server_addr[i].sin_family = AF_INET;
        server_addr[i].sin_port = htons(atoi(port)+i);
        server_addr[i].sin_addr = *((struct in_addr *) host->h_addr);
        bzero(&(server_addr[i].sin_zero), 8);
    }
    pthread_create(&sender,NULL,sender_hello,"");
    pthread_create(&recver,NULL,recver_fun,"");
    /*int i=0;
    while(i<10){
        sleep(1);
        i++;
        pthread_mutex_lock(&mutex);
        printf("Main %d\n",t);
        t++;
        pthread_mutex_unlock(&mutex);
    }*/
    pthread_join(sender, NULL /* void ** return value could go here */);

}