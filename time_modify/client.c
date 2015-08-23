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

void inline timeval_sub(struct timeval *t1, struct timeval *t2,
        struct timeval *t)
{
    t->tv_sec = t2->tv_sec - t1->tv_sec;
    t->tv_usec = t2->tv_usec - t1->tv_usec;
    if (t->tv_usec < 0)
    {
        t->tv_usec += 1000000;
        t->tv_sec -= 1;
    }
}

void inline timeval_add(struct timeval *t1, struct timeval *t)
{
    t1->tv_sec = t1->tv_sec + t->tv_sec;
    t1->tv_usec = t1->tv_usec + t->tv_usec;
    if (t1->tv_usec > 1000000)
    {
        t->tv_usec -= 1000000;
        t->tv_sec += 1;
    }
}

void inline timeval_dev(struct timeval *t, int d)
{
    int s = t->tv_sec % d;
    if (s)
    {
        t->tv_sec -= s;
        t->tv_usec += 1000000 * s;
    }
    t->tv_sec /= d;
    t->tv_usec /= d;
}

void get_delay(struct timeval *t1, struct timeval *t2, struct timeval *ts,
        struct timeval *dt)
{
    struct timeval t;
    timeval_sub(t1, t2, &t);
    timeval_dev(&t, 2);
    timeval_add(t1, &t);
    timeval_sub(t1, ts, dt);
}

int main(int argc, char *argv[])
{
    int s, i;
    socklen_t addr_len;
    char buf[64];
    struct sockaddr_in server_addr;
    struct sockaddr_in tmp_addr;
    int broadcast = 1;
    struct timeval t1, t2;
    struct timeval dt[2];
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(-1);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1989);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("bind error \n");
        exit(0);
    }
    
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
    {
        printf("setsockopt time out error \n");
        exit(0);
    }
    
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(int))< 0)
    {
        printf("setsockopt broadcast error \n");
        exit(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.255");
    server_addr.sin_port = htons(1984);
    addr_len = sizeof(server_addr);

    i = 10;
    dt[0].tv_sec = 0;
    dt[0].tv_usec = 0;
    while (i)
    {
        gettimeofday(&t1, NULL);
        if (sendto(s, buf, 64, 0, (struct sockaddr*) &server_addr,
                sizeof(server_addr)) < 0)
        {
            perror("sendto error \n");
            exit(-1);
        }
        if (recvfrom(s, buf, 64, 0, (struct sockaddr*) &tmp_addr, &addr_len)
                < 0)
        {
            printf("recvfrom time out  \n");
            continue;
        }
        gettimeofday(&t2, NULL);
        get_delay(&t1, &t2, (struct timeval *) buf, &dt[1]);
        printf("delay : %d us \n", dt[1].tv_sec * 1000000 + dt[1].tv_usec);
        timeval_add(&dt[0], &dt[1]);
        i--;
        sleep(1);
    }
    timeval_dev(&dt[0], 10);
    printf("avg delay : %d us\n", dt[0].tv_sec * 1000000 + dt[0].tv_usec);
    gettimeofday(&t1, NULL);
    timeval_add(&t1, &dt[0]);
    settimeofday(&t1, NULL);
    close(s);
    return 0;
}
