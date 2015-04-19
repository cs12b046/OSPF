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
#define INF 99999
/*
Command Line Arguments
Port No :argv[1]
IP address :argv[2]
 */
int hello_iterval,lsa_interval,spf_interval;
int t=0;
int node_id = 2;
char port[1024]="20000";
char host_name[1024] = "localhost";
int *sock;
int n,e;  //number of nodes
int** data;
int* neighbour_and_cost;
int neighbour = 0;
char** lsa_data;
int* last_seq_no;
struct sockaddr_in *server_addr;
char outfile[100];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t neighbour_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lsa_data_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t parent_lock = PTHREAD_MUTEX_INITIALIZER;
int* parent;
int* cost;
char** paths;
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
                sleep(hello_iterval);
                strcpy(str,"");
                strcpy(send_str,"");
            }
            if(data[i][1]==node_id){
                sprintf(str, "%d", node_id);
                strcat(send_str,"HELLO-");
                strcat(send_str,str);
                sendto(sock[node_id],send_str, strlen(send_str), 0,
                    (struct sockaddr *) &server_addr[data[i][0]], sizeof (struct sockaddr));
                sleep(hello_iterval);
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
    struct sockaddr_in server_addr_r, client_addr;

    if ((sock_recv = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }

    server_addr_r.sin_family = AF_INET;
    server_addr_r.sin_port = htons(atoi(port)+node_id);
    server_addr_r.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr_r.sin_zero), 8);

    if (bind(sock_recv, (struct sockaddr *) &server_addr_r,
            sizeof (struct sockaddr)) == -1) {
        perror("Bind");
        exit(1);
    }

    addr_len = sizeof (struct sockaddr);

    printf("\nUDPServer Waiting for client on port %d\n", atoi(port)+node_id);
    fflush(stdout);
    char *packet_type;
    char* packet_number;
    int send_reply_to;
    char send_reply_string[1024];
    char buffer_from[12];
    char buffer_to[12];
    int random_number,diff;
    char buffer_rand[12];
    char lsa_another[1024];
    int it;
    while (1) {
        bytes_read = recvfrom(sock_recv, recv_data, 1024, 0,
                (struct sockaddr *) &client_addr, &addr_len);


        recv_data[bytes_read] = '\0';
        printf("DATA:-%s\n", recv_data);
        strcpy(lsa_another,recv_data);
        packet_type = strtok(recv_data,"-");
        if(strcmp(packet_type,"HELLO")==0){
            packet_type = strtok(NULL,"-");
            send_reply_to = atoi(packet_type);
            sprintf(buffer_to,"%d",send_reply_to);
            sprintf(buffer_from,"%d",node_id);
            strcat(send_reply_string,"HELLOREPLY-");
            strcat(send_reply_string,buffer_from);
            strcat(send_reply_string,"-");
            strcat(send_reply_string,buffer_to);
            strcat(send_reply_string,"-");
            //put min
            for(it=0;it<e;it++){
                if(data[it][0] == node_id && data[it][1]==send_reply_to)
                    random_number = rand()%(data[it][3]-data[it][2]+1)+data[it][2];
                if(data[it][1] == node_id && data[it][0]==send_reply_to)
                    random_number = rand()%(data[it][3]-data[it][2]+1)+data[it][2];
            }
            sprintf(buffer_rand,"%d",random_number);
            strcat(send_reply_string,buffer_rand);
            sendto(sock[send_reply_to],send_reply_string, strlen(send_reply_string), 0,
                    (struct sockaddr *) &server_addr[send_reply_to], sizeof (struct sockaddr));
            strcpy(send_reply_string,"");
        }
        if(strcmp(packet_type,"HELLOREPLY")==0){
            //put lock
            packet_number = strtok(NULL,"-"); //will give neighbour
            packet_type = strtok(NULL,"-"); //will give cost
            packet_type = strtok(NULL,"-");
            pthread_mutex_lock(&neighbour_lock);
            neighbour_and_cost[atoi(packet_number)] = atoi(packet_type);
            pthread_mutex_unlock(&neighbour_lock);
            //release lock
        }
        if(strcmp(packet_type,"LSA")==0){
            int source;
            int seq;
            int nbr;
            int it;
            packet_type = strtok(NULL,"-");
            sscanf(packet_type,"%d %d %d",&source,&seq,&nbr);
            if(last_seq_no[source] < seq){
                pthread_mutex_lock(&lsa_data_lock);
                strcpy(lsa_data[source],packet_type);
                pthread_mutex_unlock(&lsa_data_lock);
                last_seq_no[source] = seq;
                pthread_mutex_lock(&neighbour_lock);
                for(it=0;it<n;it++){
                    if(neighbour_and_cost[it] !=INF && it!=node_id){
                            sendto(sock[it],lsa_another, strlen(lsa_another), 0,
                    (struct sockaddr *) &server_addr[it], sizeof (struct sockaddr));
                    }
                }
                pthread_mutex_unlock(&neighbour_lock);
            }
            //printf("++++++++++++++++++++++++++++++> %d",source);
        }

        //printf(/"\n(%s , %d) said : ", inet_ntoa(client_addr.sin_addr),
        //        ntohs(client_addr.sin_port));
        fflush(stdout);
    }
    
    return 0;
}
void *sender_lsa_fun(void *arg){
    char send_lsa[1024];
    char send_lsa_header[1024];
    char neighbour_cost_it[13];
    char total[12];
    int total_ent=0;
    int seq_no = 0;
    int it;
    //printf("THIS IS WORKING..\n");
    while(1){
        total_ent = 0;
        sprintf(send_lsa_header,"LSA-%d %d ",node_id,seq_no);
        pthread_mutex_lock(&neighbour_lock);
        /*for(it=0;it<n;it++){
            printf("Node:%d COST:%d\n",it,neighbour_and_cost[it]);
        }*/
        for(it=0;it<n;it++){
            if(neighbour_and_cost[it] != INF){
                sprintf(neighbour_cost_it," %d %d",it,neighbour_and_cost[it]);
                strcat(send_lsa,neighbour_cost_it);
                total_ent++;
            }
        }
        sprintf(total,"%d",total_ent);
        strcat(send_lsa_header,total);
        strcat(send_lsa_header,send_lsa);
        //printf("LSA PKT:%s\n",send_lsa);
        for(it=0;it<n;it++){
            if(neighbour_cost_it[it] !=INF && it!=node_id){
                sendto(sock[it],send_lsa_header, strlen(send_lsa_header), 0,
                    (struct sockaddr *) &server_addr[it], sizeof (struct sockaddr));
            }       
        }
        strcpy(send_lsa_header,"");
        strcpy(send_lsa,"");
        pthread_mutex_unlock(&neighbour_lock);
        //printf("I AM SENDING:%s\n",send_lsa);
        seq_no++;
        sleep(lsa_interval);
    }
}
int minDistance(int dist[], int sptSet[])
{
   // Initialize min value
   int min = INF;
   int min_index;
   int v;
   for (v = 0; v < n; v++){
        if (sptSet[v] == 0 && dist[v] <= min){
             min = dist[v];
             min_index = v;
        }
    }
   return min_index;
}
int printSolution(int dist[], int n)
{
    int i;
   printf("Vertex   Distance from Source\n");
   for (i = 0; i < n; i++){
      printf("%d \t\t %d\n", i, dist[i]);
      cost[i] = dist[i];
    }
}
// Funtion that implements Dijkstra's single source shortest path algorithm
// for a graph represented using adjacency matrix representation
void dijkstra_algo(int graph[n][n], int src)
{
     int dist[n];     // The output array.  dist[i] will hold the shortest
                      // distance from src to i
 
     int sptSet[n]; // sptSet[i] will true if vertex i is included in shortest
                     // path tree or shortest distance from src to i is finalized
 
     // Initialize all distances as INFINITE and stpSet[] as false
     int i;
     int v;
     for (i = 0; i < n; i++){
        dist[i] = INF;
        sptSet[i] = 0;
    }
 
     // Distance of source vertex from itself is always 0
    for(i=0;i<n;i++)
        parent[i] = i;
     dist[src] = 0;
     parent[src] = -1;
     // Find shortest path for all vertices
     int count;
     for (count = 0; count < n-1; count++)
     {
       // Pick the minimum distance vertex from the set of vertices not
       // yet processed. u is always equal to src in first iteration.
       int u = minDistance(dist, sptSet);
 
       // Mark the picked vertex as processed
       sptSet[u] = 1;
 
       // Update dist value of the adjacent vertices of the picked vertex.
       for (v = 0; v < n; v++)
 
         // Update dist[v] only if is not in sptSet, there is an edge from
         // u to v, and total weight of path from src to  v through u is
         // smaller than current value of dist[v]
         if (!sptSet[v] && graph[u][v] && dist[u] != INF
                                       && dist[u]+graph[u][v] < dist[v]){
            dist[v] = dist[u] + graph[u][v];
            pthread_mutex_lock(&parent_lock);
            parent[v] = u;
            pthread_mutex_unlock(&parent_lock);
        }
     }
 
     // print the constructed distance array
     printSolution(dist, n);
}


void *Dijkestra(void *arg){
    int it=0;
    int i,j;
    int source;
    int number;
    int seq;
    int pos;
    int node,weight;
    int adj[n][n];
    int path[n];
    int shortdist;
    int remove_this;
    int time_now =0;
    while(1){
        pthread_mutex_lock(&lsa_data_lock);
        for(i=0;i<n;i++){
            for(j=0;j<n;j++){
                adj[i][i] = 0;
                if(i!=j)
                    adj[i][j] =INF;
            }
        }
        for(it=0;it<n;it++){
            int curr;
            //printf("*********************** %s\n",lsa_data[it]);
            sscanf(lsa_data[it],"%d %d %d %n",&source,&seq,&number,&pos);
            for(i=0;i<number;i++){
                sscanf(lsa_data[it]+pos,"%d %d %n",&node,&weight,&curr);
                adj[source][node] = weight;
                pos+=curr;
            }

        }
        pthread_mutex_unlock(&lsa_data_lock);
        for(i=0;i<n;i++){
            for(j=0;j<n;j++){
                printf("%d\t",adj[i][j]);
            }
            printf("\n");
        }
        dijkstra_algo(adj, node_id);
        pthread_mutex_lock(&parent_lock);
        printf("PARENT: ");
        for(i = 0;i<n;i++){
            printf("%d ",parent[i]);
        }
        printf("\n");
        int path_local[n];
        int it_local;
        int put_in = 0;
        char local_path_st[1024];
        char buffer_local[12];
        for(i=0;i<n;i++){
            put_in = 0;
            for(it_local = 0;it_local<n;it_local++){
                path_local[it_local] = -11;
            }
            //printf("PATH %d : ",i);
            if(parent[i] == -1){
                printf("%d\n",i);
            }
            else if(parent[i] == i){
                printf("PDNE\n");
            }
            else{
                    remove_this = parent[i];
                    while(remove_this != -1 ){
                        //printf("%d ",remove_this);
                        path_local[put_in] = remove_this;
                        put_in++;
                        remove_this = parent[remove_this];
                    }
                    //printf("\n");
                    for(it_local = n-1;it_local>=0;it_local--){
                        if(path_local[it_local] == -11)
                            continue;
                        sprintf(buffer_local,"%d",path_local[it_local]);
                        strcat(local_path_st,buffer_local);
                        strcat(local_path_st,"-");
                    }
                    sprintf(buffer_local,"%d",i);
                    strcat(local_path_st,buffer_local);
                    //printf("%s\n",local_path_st);
                    strcpy(paths[i],local_path_st);
                    strcpy(local_path_st,"");
            }
            
        }
        printf("\n");
        pthread_mutex_unlock(&parent_lock);
        int pr_i;
        char file_write[100];
        sprintf(file_write,"%s-%d.txt",outfile,node_id);
        FILE *file;
        file = fopen(file_write,"w");
        fprintf(file,"Routing Table for Node No. %d at Time %d \n",node_id,time_now);
        fprintf(file,"DESINATION PATH COST\n");
        for(pr_i=0;pr_i<n;pr_i++){
            if(pr_i == node_id)
                continue;
            fprintf(file,"%d \t%s \t%d\n",pr_i,paths[pr_i],cost[pr_i]);
        }
        //lcost=dij(adj,n,0,3);
        fclose(file);
        time_now +=spf_interval;
        sleep(spf_interval);
    }

}

int main(int argc, char *argv[]) {
    pthread_t sender;
    pthread_t recver;
    pthread_t sender_lsa;
    pthread_t dijkestra;
    char input_file[1024];
    //printf("%d %d",n,e);
    int j,k;
    //printf("%d",argc);
    hello_iterval = 1;
    lsa_interval = 5;
    spf_interval = 20;
    for(j=1;j<argc;j++){
        if(strcmp(argv[j],"-i")==0){
            node_id = atoi(argv[j+1]);
            printf("Node id is: %d\n",node_id);
            j++;
        }
        else if(strcmp(argv[j],"-f")==0){
            strcpy(input_file,argv[j+1]);  
            printf("Input file: %s\n",input_file); 
            j++;
        }
        else if(strcmp(argv[j],"-o")==0){
            strcpy(outfile,argv[j+1]);
            printf("Out file: %s\n",outfile); 
            j++;
        }
        else if(strcmp(argv[j],"-h")==0){
            hello_iterval = atoi(argv[j+1]);
            printf("hello :%d\n",hello_iterval);
            j++;
        }
        else if(strcmp(argv[j],"-a")==0){
            lsa_interval = atoi(argv[j+1]);
            printf("lsa :%d\n",lsa_interval);
            j++;
        }
        else if(strcmp(argv[j],"-s")==0){
            spf_interval = atoi(argv[j+1]);
            printf("Dtime :%d\n",spf_interval);
            j++;
        }
        else{
            printf("Invelid input..");
            exit(0);
        }
    }
    FILE *fp;
    fp = fopen(input_file,"r");
    fscanf(fp,"%d %d",&n,&e);
    data = (int **)malloc(e*sizeof(int*));
    for(j=0;j<e;j++){
        *(data+j) = (int *) malloc(4*sizeof(int));
        for(k=0;k<4;k++){
            data[j][k] = 0;
        }

    }
    neighbour_and_cost = (int *)malloc(n*sizeof(int));
    for(j=0;j<e;j++){
        neighbour_and_cost[j] = INF;
    }
    for(j=0;j<e;j++){
        //*(data+j) = (int *) malloc(4*sizeof(int));
        for(k=0;k<4;k++){
            fscanf(fp,"%d",&data[j][k]);
        }

    }
    for(j=0;j<e;j++){
                if(data[j][0]==node_id ||data[j][1]==node_id)
                    neighbour++;
    }
    lsa_data = (char **)malloc(n*sizeof(char *));
    last_seq_no = (int* )malloc(n*sizeof(int));
    for(j=0;j<n;j++){
        *(lsa_data+j) = (char *)malloc(1024);
        strcpy(lsa_data[j],""); 
        last_seq_no[j] = -1;
    }
    parent = (int*)malloc(n*sizeof(int));
    for(j=0;j<n;j++){
        parent[j] = j;
    }

    cost = (int *)malloc(n*sizeof(int));
    paths = (char **)malloc(n*sizeof(char*));
    for(j = 0;j<n;j++){
        *(paths+j) = (char *)malloc(1024*sizeof(char));
    }
    //printf("DATA[0] %d %d %d %d",data[0][0],data[0][1],data[0][2],data[0][3]);
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
    pthread_create(&sender_lsa,NULL,sender_lsa_fun,"");
    pthread_create(&dijkestra,NULL,Dijkestra,"");
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
    pthread_join(dijkestra, NULL /* void ** return value could go here */);
}