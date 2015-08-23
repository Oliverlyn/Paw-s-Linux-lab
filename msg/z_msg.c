#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <semaphore.h>
#include "z_msg.h"

static int msgids[256];
static sem_t sem;

void initmsg(int nub)
{
	sem_wait(&sem);
	if(msgids[nub] > -1)
		goto out;
	msgids[nub] = msgget((key_t)MYMSG_KEY + nub, 0666 | IPC_CREAT);
	if(msgids[nub] < 0)
	{
		perror("msgget ERROR ");
		exit(1);
	}
out:
	sem_post(&sem);
}

void printm(int nub,struct msg_st *m)
{
	if(nub > 255 || nub < 0 || !m)
	{
		printf("printm error !\n");
		exit(1);
	}
	if(msgids[nub] < 0)
		initmsg(nub);
	m->msg_type = 1984;
	if(msgsnd(msgids[nub], (void*)m, 256, IPC_NOWAIT) < 0)
	{
		perror("msgsnd ERROR ");
		exit(1);
	}
}

static void __attribute__((constructor)) _init(void)
{
	int i = 0;
	while(i<256)
		msgids[i++] = -11;
	sem_init(&sem, 0, 1);
}
