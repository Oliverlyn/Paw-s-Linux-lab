#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include "z_msg.h"

int main(int len,char **a)
{
	int nub;
	int msgid;
	struct msg_st data;
	if(len != 2)
	{
		printf("Usage: msg_recv [1-256] \n");
		exit(0);
	}
	nub = atoi(a[1]);
	if(nub > 256 || nub < 1)
	{
		printf("Usage: msg_recv [1-256] \n");
		exit(0);
	}
	nub --;
	msgid = msgget((key_t)MYMSG_KEY + nub, 0666 | IPC_CREAT);
	if(msgid < 0)
	{
		perror("msgget ERROR ");
		exit(1);
	}
	while(1)
	{
        if(msgrcv(msgid, (void*)&data, 256, 0, 0) < 0)
        {
            perror("msgrcv ERROR ");
            exit(1);
        }
        printf("%s",data.data);
        fflush(0);
	}
}
