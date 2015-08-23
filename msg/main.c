#include <stdio.h>
#include <sys/socket.h>
#include "z_msg.h"

#define  close(x) do{m_n(256,"close %d in %s at %d \n",x, __FILE__,__LINE__);close(x);}while(0)

int main()
{

	int i = 0;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	close(sock);
	while(i < 20)
	{
		
#define printf m_01
		printf("out2 : %d\n",i);
#undef printf
		
#define printf m_02
		printf("out3 : %d\n",i);
#undef printf
		
#define printf m_03
		printf("out4 : %d\n",i);
#undef printf
		
#define printf m_04
		printf("out5 : %d\n",i);
#undef printf
		
		printf("out1 : %d\n",i);
		i++;
		sleep(1);
	}
	return 0;
}