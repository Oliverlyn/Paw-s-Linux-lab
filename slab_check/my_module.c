#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/netpoll.h>
#include <linux/preempt.h>
#include <linux/numa.h>
#include <linux/slab.h>

//#define kmem_size 896*1024*256

#define _L unsigned long
#define _K(x) ((_L)x>0xc0000000 && (_L)x<0xf8000000)
struct my_list {
	struct my_list *next, *prev;
	void *data1, *data2, *data3;
};
struct list_head *cache_chain = (void *) 0xc07cc6ec;
struct mutex *cache_chain_mutex = (void *) 0xc0697818;

struct kmem_list3 {
	struct list_head slabs_partial;
	struct list_head slabs_full;
	struct list_head slabs_free;
	unsigned long free_objects;
	unsigned int free_limit;
	unsigned int colour_next;
	spinlock_t list_lock;
	struct array_cache *shared;
	struct array_cache **alien;
	unsigned long next_reap;
	int free_touched;
};
struct _kmem_cache {
	struct array_cache *array[NR_CPUS];
	unsigned int batchcount;
	unsigned int limit;
	unsigned int shared;
	unsigned int buffer_size;
	struct kmem_list3 *nodelists[MAX_NUMNODES];
	unsigned int flags;
	unsigned int num;
	unsigned int gfporder;
	gfp_t gfpflags;
	size_t colour;
	unsigned int colour_off;
	struct _kmem_cache *slabp_cache;
	unsigned int slab_size;
	unsigned int dflags;
	void (*ctor)(void *, struct _kmem_cache *, unsigned long);
	void (*dtor)(void *, struct _kmem_cache *, unsigned long);
	const char *name;
	struct list_head next;
};

void show_e(struct list_head *e) {
	struct my_list *tmp[7] = { 0 };
	int i = 0;
	tmp[2] = (void *) e->prev;
	if (_K(tmp[2]))
		tmp[1] = tmp[2]->prev;
	if (_K(tmp[1]))
		tmp[0] = tmp[1]->prev;
	tmp[3] = (void *) e;
	tmp[4] = (void *) e->next;
	if (_K(tmp[4]))
		tmp[5] = tmp[4]->next;
	if (_K(tmp[5]))
		tmp[6] = tmp[5]->next;
	printk("list_head:");
	while (i < 7)
		printk("  %p", tmp[i++]);
	printk("\n");
	printk("next_head:");
	i = 0;
	while (i < 7) {
		printk("  %p", (_K(tmp[i]) ? tmp[i]->next : tmp[i]));
		i++;
	}
	printk("\n");
	printk("prev_head:");
	i = 0;
	while (i < 7) {
		printk("  %p", (_K(tmp[i]) ? tmp[i]->prev : tmp[i]));
		i++;
	}
	printk("\n");
	printk("data1:    ");
	i = 0;
	while (i < 7) {
		printk("  %p", (_K(tmp[i]) ? tmp[i]->data1 : tmp[i]));
		i++;
	}
	printk("\n");
	printk("data2:    ");
	i = 0;
	while (i < 7) {
		printk("  %p", (_K(tmp[i]) ? tmp[i]->data2 : tmp[i]));
		i++;
	}
	printk("\n");
	printk("data3:    ");
	i = 0;
	while (i < 7) {
		printk("  %p", (_K(tmp[i]) ? tmp[i]->data3 : tmp[i]));
		i++;
	}
	printk("\n");
}

static int check_list(struct list_head *e) {
	struct list_head *pos = e;
	int steps = 0;
	while (steps < 65535) {
		steps++;
		if (unlikely(pos->next->prev != pos)) {
			printk(KERN_ERR"next->prev should %p but %p\n",e,e->next->prev);
			printk(KERN_ERR"--------------check_list---------------\n");
			show_e(e);
			return -1;
		}
		pos = pos->next;
		if (unlikely(pos == e))
			return 0;
	}
	printk(KERN_ERR"Too many steps\n");
	return -2;
}

static void slab_checker(void) {
	struct _kmem_cache *searchp;
	struct kmem_list3 *l3;
	int i;
	mutex_lock(cache_chain_mutex);
	list_for_each_entry(searchp, cache_chain, next)
	{
		printk("--z-- checking %s\n", searchp->name);
		for (i = 0; i < MAX_NUMNODES; i++) {
			l3 = searchp->nodelists[i];
			check_list(&l3->slabs_partial);
			check_list(&l3->slabs_full);
			check_list(&l3->slabs_free);
		}
	}
	mutex_unlock(cache_chain_mutex);
}

/*
 struct kernel_mem
 {
 unsigned long mem[kmem_size];
 };

 struct print_mem
 {
 unsigned long mem[9];
 };

 static int _init_module(void)
 {
 struct kernel_mem *mem = (void *)0xc0000000;
 struct print_mem *p_mem;
 int i = 4;
 int j = 0;
 for(;i<kmem_size;i++)
 {
 if(unlikely(mem->mem[i] == 0x01010101))
 {
 printk("--------0101---------\n");
 p_mem = (void *)(&mem->mem[i-4]);
 for(j=0;j<9;j++)
 printk("%p : %08lx \n",&p_mem->mem[j],p_mem->mem[j]);
 printk("--------end----------\n");
 i+=4;
 }
 }
 printk("replace_fun OK %p\n",&mem->mem[kmem_size]);
 return 0;
 }
 */
static int _init_module(void) {
	slab_checker();
	printk("init \n");
	return 0;
}
static void _cleanup_module(void) {

	printk("+ Unloading module\n");
}

module_init( _init_module);
module_exit( _cleanup_module);
