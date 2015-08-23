#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/time.h>

int main(int argc,char *argv[])
{
    int s;
    socklen_t addr_len;
    char buf[64];
    struct sockaddr_in server_addr;

    if((s = socket(PF_INET,SOCK_DGRAM,0)) < 0)
    {
        perror("socket error\n");
        exit(-1);
    }
 
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    server_addr.sin_port = htons(1984);
    addr_len=sizeof(server_addr);
 
    if(bind(s,(struct sockaddr*)&server_addr,addr_len) < 0)
    {
        perror("bind error\n");
        exit(-1);
    }
    while(1)
    {
        if(recvfrom(s,buf,64,0,(struct sockaddr*)&server_addr,&addr_len) < 0)
        {
            perror("recvfrom error \n");
            exit(-1);
        }
        gettimeofday((struct timeval *)buf,NULL);
        if(sendto(s,buf,64,0,(struct sockaddr*)&server_addr,addr_len) < 0)
        {
            perror("sendrto error \n");
            exit(-1);
        }
    }
 
    close(s);
    return 0;
}
