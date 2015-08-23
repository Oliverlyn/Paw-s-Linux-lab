#ifndef Z_MAG
#define Z_MAG

#define MYMSG_KEY 10086

struct msg_st
{
    long msg_type;
    char data[256];
};

void printm(int nub,struct msg_st *m);

#define  m_01(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(0,&_z_tmp);}while(0)
#define  m_02(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(1,&_z_tmp);}while(0)
#define  m_03(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(2,&_z_tmp);}while(0)
#define  m_04(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(3,&_z_tmp);}while(0)
#define  m_05(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(4,&_z_tmp);}while(0)
#define  m_06(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(5,&_z_tmp);}while(0)
#define  m_07(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(6,&_z_tmp);}while(0)
#define  m_08(x,...) do{struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(7,&_z_tmp);}while(0)
#define  m_n(n,x,...) do{if(n>256||n<1){fprintf(stdout,"N error\n");break;}struct msg_st _z_tmp;snprintf(_z_tmp.data,256,x,## __VA_ARGS__);printm(n-1,&_z_tmp);}while(0)

#endif

