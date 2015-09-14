#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/desc.h>
#include <linux/highmem.h>
#include <linux/string.h>

typedef struct desc_struct gate_desc;
#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

struct desc_easy {
	unsigned short a;
	unsigned short b;
	unsigned short c;
	unsigned short d;
} __attribute__((packed)) ;

struct desc_ptr {
        unsigned short size;
        unsigned long address;
} __attribute__((packed)) ;

#define PGFAULT_INT 0x0E

static unsigned long do_page_fault;
static unsigned long ret_from_exception;
static struct desc_ptr old_idtr;
static unsigned long kpage;

asmlinkage void my_function(void);

//#define printk(l, ...) ;

void stub(void)
{
	__asm__
	(
			"	 pushl  do_page_fault        \n"
			"	 pushl  %ds                  \n"
			"	 pushl  %eax                 \n"
			"	 xor    %eax,%eax            \n"
			"	 pushl  %ebp                 \n"
			"	 pushl  %edi                 \n"
			"	 pushl  %esi                 \n"
			"	 pushl  %edx                 \n"
			"	 dec    %eax                 \n"
			"	 pushl  %ecx                 \n"
			"	 pushl  %ebx                 \n"
			"	 cld                         \n"
			"	 pushl  %es                  \n"
			"	 pushl  %eax                 \n"
			"	 popl   %eax                 \n"
			"	 popl   %ecx                 \n"
			"	 movl   0x20(%esp),%edi      \n"
			"	 movl   0x24(%esp),%edx      \n"
			"	 movl   %eax,0x24(%esp)      \n"
			"	 movl   %ecx,0x20(%esp)      \n"
			"	 movl   $0x7b,%ecx    		 \n" //__user_ds in linux 2.6
			"	 movl   %ecx,%ds             \n"
			"	 movl   %ecx,%es             \n"
			"	 movl   %esp,%eax            \n"
			"	 call   *do_page_fault       \n"
			"	 pushal 				     \n"
			"	 pushl 	%es			         \n"
			"	 pushl 	%ds			         \n"
			"	 movl   $0x68,%ecx  		 \n" //__kernel_ds in linux 2.6
			"	 movl   %ecx,%ds             \n"
			"	 movl   %ecx,%es             \n"
			"	 call 	my_function	         \n"
			"	 popl 	%ds			         \n"
			"	 popl 	%es			         \n"
			"	 popal				         \n"
			"	 jmp    *ret_from_exception  \n"
			"	 xchg   %ax,%ax              \n"
	);
}

void set_address_zero(unsigned long address)
{
        pgd_t *pgd;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;
        struct page *pg;
        s8 *k_addr;

        spin_lock(&current->mm->page_table_lock);
        pgd = pgd_offset(current->mm,address);
        pud = pud_offset(pgd, address);
        if (!pud)
        {
        	printk("2\n");
        	goto out;
        }
        pmd = pmd_offset(pud, address);
        if (!pmd)
        {
        	printk("3\n");
        	goto out;
        }
        pte = pte_offset_map(pmd, address);
        if (pte_present(*pte))
        {
        	pg = pte_page(*pte);
        	pte_unmap(pte);
        	if(!IS_ERR(pg))
        	{

        		k_addr = kmap_atomic(pg, KM_USER1);
        		printk("Find Page %08x  ,,,set ALL ZERO !\n",(unsigned int )k_addr);
        		memset(k_addr,0,PAGE_SIZE);
        		kunmap_atomic(k_addr, KM_USER1);
        		goto out;
        	}
        	else
            {
            	printk("5\n");
            	goto out;
            }
        }
        else
        {
        	pte_unmap(pte);
        	printk("4\n");
        	goto out;
        }
out:
        spin_unlock(&current->mm->page_table_lock);
}

asmlinkage void my_function(void)
{
	unsigned long add;
	if(strcmp(current->comm,"aaa.a"))
		return;
	asm("movl %%cr2,%0":"=r"(add));
	if(add > current->mm->start_brk + PAGE_SIZE && add < current->mm->start_stack - PAGE_SIZE)
	{
		printk("PID: %d >> %08x\n",current->tgid,(unsigned int )add);
		//spin_lock(&current->mm->page_table_lock);
		set_address_zero(add);
		//spin_unlock(&current->mm->page_table_lock);
		//memset((void *)(add - (add&(PAGE_SIZE-1))),0,PAGE_SIZE);
	}
}

int pgfault_init( void )
{
    gate_desc *idt_table;
    struct desc_ptr new_idtr;
    unsigned char * prt,* off;
    unsigned long isr_orig;
    unsigned long isr_new;
    unsigned long error_code;
    gate_desc *PF_gate;
    int i,pos = 0;

    printk("+z+ pgfault_init\n");

    //获取IDT指针
    //asm ("sidt %0" : "=m" (old_idtr));
    asm ("sidt %0" : "=m" (new_idtr));
    old_idtr = new_idtr;

    kpage =get_zeroed_page(GFP_KERNEL);
    if ( !kpage )
    	return -ENOMEM;

    new_idtr.address = kpage;
    memcpy((void *)kpage,(void *)old_idtr.address,PAGE_SIZE);

    idt_table = ((gate_desc *) new_idtr.address);

    //page_fault_gate地址
    PF_gate = &idt_table[PGFAULT_INT];

    //原始的page_fault地址
    isr_orig = (PF_gate->a & 0xffff) | (PF_gate->b & 0xffff0000);
    //找到do_page_fault()、error_code、ret_from_exception函数的地址
    do_page_fault = *(unsigned long *)(isr_orig + 1);//push
    error_code = (isr_orig + 10 + *(int*)(isr_orig +6));//jmp
    prt = (unsigned char *) error_code;
    for(i = 0; i < 96; i++) {
        off = prt + i;
        if(*(off) == 0xff && *(off+1) == 0xd7 && *(off+2) == 0xe9) {
            pos = *(int*)(off+3);//jmp
            pos += (i + 7);
            i = -1;
            break;
        }
    }
    if(i >= 0)
    {
    	printk("error in find ret_from_exception \n");
    	return 0;
    }
    ret_from_exception = error_code + pos;
    //把新的处理函数地址填充进去

    //return 0;
    isr_new = (unsigned long)&stub;
    ((struct desc_easy *) PF_gate)->a = (unsigned short) (isr_new & 0x0000FFFF);
    ((struct desc_easy *) PF_gate)->d = (unsigned short) (isr_new >> 16);

    asm(" lidt %0 " : : "m" (new_idtr) );

    return 0;
}

void pgfault_exit( void )
{
	printk("+z+ pgfault_exit\n");
	//还原以前的page_fault地址
	//((struct desc_easy *) PF_gate)->a = (unsigned short) (isr_orig & 0x0000FFFF);
	//((struct desc_easy *) PF_gate)->d = (unsigned short) (isr_orig >> 16);
	asm(" lidt %0 " : : "m" (old_idtr) );
	if ( kpage )
		free_page( kpage );
}

MODULE_LICENSE("GPL");
module_init( pgfault_init);
module_exit( pgfault_exit);
