#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/netpoll.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define _L unsigned long
#define _K(x) ((_L)x>0xc0000000)
struct my_list
{
	struct my_list *next, *prev;
	void *data1,*data2,*data3;
};
///////////UDP///////////////
#define INADDR ((unsigned long int)0xc0a80100) //192.168.1.0
//#define INADDR_SEND ((unsigned long int)0xac10b287) //172.16.178.135
static struct netpoll* np = NULL;
static struct netpoll np_t;
static int local = 0;
module_param(local, int, 0666);
///////////hook//////////////
#define PANIC_ADDR 0xc0426a10
#define BUST_SPIN 0xc041e097
#define PMD_BAD 0xc046483f
#define CACHE_REAP_ADDR 0xc04758aa
#define DRAIN_ARRAY_ADDR 0xc04744c8
#define KMEM_ALLOC_ADDR 0xc04746d4
#define CACHE_REFILL_ADDR 0xc047471f
#define LIST_DEL_ADDR 0xc04f7ed8
unsigned char list_del_old[5];
typedef void (*o_bust_spinlocks)(int yes);
#define CLEAR_CR0    asm ("pushl %eax\n\t"             \
"movl %cr0, %eax\n\t"        \
"andl $0xfffeffff, %eax\n\t"     \
"movl %eax, %cr0\n\t"        \
"popl %eax");
#define SET_CR0        asm ("pushl %eax\n\t"             \
"movl %cr0, %eax\n\t"         \
"orl $0x00010000, %eax\n\t"     \
"movl %eax, %cr0\n\t"        \
"popl %eax");

MODULE_LICENSE("GPL");

/////////////////////////////
///////////UDP///////////////
/////////////////////////////

static void fake_poll(struct net_device *dev)
{
	msleep(1);
}

void init_netpoll(void)
{
	struct net_device *ndev = NULL;
	np_t.name = "--z--";
	strlcpy(np_t.dev_name, "wlan0", IFNAMSIZ);
	np_t.local_ip = (INADDR + local);
	np_t.remote_ip = (INADDR + 4);
	np_t.local_port = 19840;
	np_t.remote_port = 19840 + local;
	memset(np_t.remote_mac, 0xff, ETH_ALEN);
	//netpoll_print_options(&np_t);
	ndev = dev_get_by_name(np_t.dev_name);
	if (ndev && !ndev->poll_controller)
	{
		ndev->poll_controller = fake_poll;
		netpoll_setup(&np_t);
		//ndev->poll_controller = NULL;
	}
	else
		netpoll_setup(&np_t);
	np = &np_t;
}

void send_udp(char *buffer)
{
	int len = strlen(buffer) + 1;
	int pos = 0;
	while(pos < len)
	{
		netpoll_send_udp(np,buffer + pos,(pos+256 < len)?256:(len-pos));
		pos += 256;
	}
}

/////////////////////////////
///////////hook//////////////
/////////////////////////////

void show_e(struct list_head *e)
{
	struct my_list *tmp[7] = {0};
	int i = 0;
	tmp[2] = (void *)e->prev;
	if(_K(tmp[2]))
		tmp[1] = tmp[2]->prev;
	if(_K(tmp[1]))
		tmp[0] = tmp[1]->prev;
	tmp[3] = (void *)e;
	tmp[4] = (void *)e->next;
	if(_K(tmp[4]))
		tmp[5] = tmp[4]->next;
	if(_K(tmp[5]))
		tmp[6] = tmp[5]->next;
	printk("list_head:");
	while(i < 7)
		printk("  %p",tmp[i++]);
	printk("\n");
	printk("next_head:");
	i = 0;
	while(i < 7)
	{
		printk("  %p",(_K(tmp[i])?tmp[i]->next:tmp[i]));
		i++;
	}
	printk("\n");
	printk("prev_head:");
	i=0;
	while(i < 7)
	{
		printk("  %p",(_K(tmp[i])?tmp[i]->prev:tmp[i]));
		i++;
	}
	printk("\n");
	printk("data1:    ");
	i=0;
	while(i < 7)
	{
		printk("  %p",(_K(tmp[i])?tmp[i]->data1:tmp[i]));
		i++;
	}
	printk("\n");
	printk("data2:    ");
	i=0;
	while(i < 7)
	{
		printk("  %p",(_K(tmp[i])?tmp[i]->data2:tmp[i]));
		i++;
	}
	printk("\n");
	printk("data3:    ");
	i=0;
	while(i < 7)
	{
		printk("  %p",(_K(tmp[i])?tmp[i]->data3:tmp[i]));
		i++;
	}
	printk("\n");
}
struct list_head * loop_walker(struct list_head *e,struct list_head *error_slab,int flag)
{
	int i = 0;
	struct list_head * pos = e;
	struct list_head * p; 
	while(i++<128)
	{
		p = pos;
		if(_K(pos))
			pos = (flag==2)?pos->prev:pos->next;
		else
			return NULL;
		if(pos == error_slab)
			return p;
	}
	return NULL;
}
int try_fix(struct list_head *e,int flag)
{
	//struct list_head * error_p = (flag==1)?e->prev->next:e->next->prev;
	struct list_head * error_slab = (flag==1)?e->prev:e->next;
	struct list_head * fix_slab = loop_walker(e,error_slab,flag);
	if(fix_slab)
	{
		printk("Find list loop,fix it !\n");
		error_slab->next = (flag==1)?e:fix_slab;
		error_slab->prev = (flag==1)?fix_slab:e;
		show_e(e);
		return 0;
	}
	else 
		printk("Can't fix,just OOPS!\n");
	return 1;
	/*
	if(error_p & 0x00000001)
	{
		fix_mask = e ^ error_p;
		printk("May be 101 error by %p ,try to fix\n",fix_mask);
		error_slab->prev ^= fix_mask;
		error_slab->next ^= fix_mask;
		show(e);
		printk("Does it looks good now? A good list must be a loop.\n");
	}
	else
	{
		printk("")
	}
	*/
}

void my_list_del(struct list_head *e)
{
	if(unlikely(e->prev->next != e))
	{
		printk(KERN_ERR"prev->next should %p but %p\n",e,e->prev->next);
		printk(KERN_ERR"--------------list_del_01-------------\n");
		show_e(e);
		if(try_fix(e,1))
			BUG();
	}
	if(unlikely(e->next->prev != e))
	{
		printk(KERN_ERR"next->prev should %p but %p\n",e,e->next->prev);
		printk(KERN_ERR"--------------list_del_02-------------\n");
		show_e(e);
		if(try_fix(e,2))
			BUG();
	}
	__list_del(e->prev,e->next);
	e->prev = (void*)0x101010;
	e->next = (void*)0x202020;
}

static void my_drain_array(void *cachep, void *l3, void *ac, int force, int node)
{
	void (*o)(void *cachep, void *l3, void *ac, int force, int node) = (void *) DRAIN_ARRAY_ADDR;
	printk("--z-- cache_reap - %s - %d\n",kmem_cache_name(cachep),smp_processor_id());
	o(cachep,l3,ac,force,node);
}

static void *my_cache_alloc_refill(void *cachep,gfp_t flags)
{
	void *(*o)(void *cachep,gfp_t flags) = (void *)CACHE_REFILL_ADDR;
	printk("--z-- kmem_cache_alloc - %-20s %d\t %-12s - %d\n",kmem_cache_name(cachep),smp_processor_id(),current->comm,current->pid);
	return o(cachep,flags);
}

int my_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int ret = vscnprintf(buf,size,fmt,args);
	if(ret > 0)
		send_udp(buf);
	return ret;
}
static void my_bust_spinlocks(int yes)
{
	o_bust_spinlocks o = (void *) BUST_SPIN;
	preempt_enable();
	printk("Got a panic!!!!\n");
	schedule();
	printk("scheduled!!!!\n");
	msleep(1000);
	printk("sleep1 over !!!!\n");
	msleep(1500);
	printk("sleep2 over !!!!\n");
	msleep(2000);
	printk("sleep3 over !!!!\n");
	schedule();
	preempt_disable();
	o(yes);
}

asmlinkage int pmd_error_printk(const char *f,const char *s,int d1,void *p,_L long d2)
{
	int ret = printk(f,s,d1,p,d2);
	printk("--z-- %d - %s - %p \n",current->pid,current->comm,current);
	printk(".................WARN...................\n");
	WARN_ON(1);
	return ret;
}

static int replace_fun(_L handle, _L old_fun, _L new_fun,int pos)
{
	unsigned char *p = (unsigned char *)handle;
	int i = 0;
	p += pos;
	while(1)
	{
		if(i++ > 128)
			return 0;
		if(*p == 0xe8)
		{
			if((*(int *)(p+1) + (_L)p + 5) == old_fun)
			{
				*(int *)(p+1) = new_fun - (_L)p - 5;
				return (_L)p - handle - pos;
			}
		}
		p++;
	}
	return 0;
}

void add_jmp(_L hook_point, _L new_fun,unsigned char *old)
{
	unsigned char *p = (void *)hook_point;
	memcpy(old,p,5);
	*p = 0xe9;
	p++;
	*(int *)p = new_fun - hook_point -5;
}

void remove_jmp(_L hook_point, unsigned char *old)
{	
	memcpy((void *)hook_point,old,5);
}

static int _init_module(void)
{
	int ret1 ,ret2 ,ret3 ,ret4 ,ret5;
	//pmd_t test;
	//void (*pmd_c)(pmd_t *pmd) = (void *)PMD_BAD;
	//test.pmd = 0x10101010;
	/*
	if(local > 3 || local <1)
	{
		printk("local error!\n");
		return -1;
	}
	*/
	//init_netpoll();
	CLEAR_CR0
	ret1 = replace_fun((_L)PANIC_ADDR,(_L)BUST_SPIN,(_L)my_bust_spinlocks,0);
	//ret2 = replace_fun((_L)vprintk,(_L)vscnprintf,(_L)my_vscnprintf,200);
	ret3 = replace_fun((_L)PMD_BAD,(_L)printk,(_L)pmd_error_printk,0);
	//ret4 = replace_fun((_L)CACHE_REAP_ADDR,(_L)DRAIN_ARRAY_ADDR,(_L)my_drain_array,50);
	//ret5 = replace_fun((_L)KMEM_ALLOC_ADDR,(_L)CACHE_REFILL_ADDR,(_L)my_cache_alloc_refill,50);
	SET_CR0
	if(!ret1 || /*!ret2 ||*/ !ret3 /*|| !ret4 || !ret5*/)
	{
		printk("replace_fun error %d - %d - %d\n",ret1,ret2,ret3);
		CLEAR_CR0
		if(ret1)
			replace_fun((_L)PANIC_ADDR,(_L)my_bust_spinlocks,(_L)BUST_SPIN,0);
		//if(ret2)
		//	replace_fun((_L)vprintk,(_L)my_vscnprintf,(_L)vscnprintf,200);
		if(ret3)
			replace_fun((_L)PMD_BAD,(_L)pmd_error_printk,(_L)printk,0);
		//if(ret4)
		//	replace_fun((_L)CACHE_REAP_ADDR,(_L)my_drain_array,(_L)DRAIN_ARRAY_ADDR,0);
		//if(ret5)
		//	replace_fun((_L)KMEM_ALLOC_ADDR,(_L)my_cache_alloc_refill,(_L)CACHE_REFILL_ADDR,0);
		SET_CR0
		return -1;
	}
	//BUG();
	CLEAR_CR0
	add_jmp((_L)LIST_DEL_ADDR,(_L)my_list_del,list_del_old);
	SET_CR0
	printk("replace_fun OK \n");
	//(*pmd_c)(&test);	
	return 0;
}

static void _cleanup_module(void)
{
	if(np)
		netpoll_cleanup(np);
	CLEAR_CR0
	replace_fun((_L)PANIC_ADDR,(_L)my_bust_spinlocks,(_L)BUST_SPIN,0);
	//replace_fun((_L)vprintk,(_L)my_vscnprintf,(_L)vscnprintf,200);
	replace_fun((_L)PMD_BAD,(_L)pmd_error_printk,(_L)printk,0);
	//replace_fun((_L)CACHE_REAP_ADDR,(_L)my_drain_array,(_L)DRAIN_ARRAY_ADDR,0);
	//replace_fun((_L)KMEM_ALLOC_ADDR,(_L)my_cache_alloc_refill,(_L)CACHE_REFILL_ADDR,0);
	remove_jmp((_L)LIST_DEL_ADDR,list_del_old);
	SET_CR0
	printk("+ Unloading module\n");
}

module_init( _init_module);
module_exit( _cleanup_module);
