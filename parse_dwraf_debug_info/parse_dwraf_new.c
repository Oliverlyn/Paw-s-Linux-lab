#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "dwarf.h"
#include "libdwarf.h"

#define printf(line, ...) ; //禁用所有的printf

static const char *const dwarf_regnames_i386[] =
{
  "eax", "ecx", "edx", "ebx",
  "esp", "ebp", "esi", "edi",
  "eip", "eflags", NULL,
  "st0", "st1", "st2", "st3",
  "st4", "st5", "st6", "st7",
  NULL, NULL,
  "xmm0", "xmm1", "xmm2", "xmm3",
  "xmm4", "xmm5", "xmm6", "xmm7",
  "mm0", "mm1", "mm2", "mm3",
  "mm4", "mm5", "mm6", "mm7",
  "fcw", "fsw", "mxcsr",
  "es", "cs", "ss", "ds", "fs", "gs", NULL, NULL,
  "tr", "ldtr"
};//寄存器顺序+名字，从GDB源代码里copy过来的。。。

static const char *const dwarf_base_type[]=
{
	"void",
	"unsigned int", "unsigned char"	,"short unsigned int",
	"long unsigned int", "signed char", "short int",
	"int", "long long int", "long long unsigned int",
	"long int", "char", "union", "struct"
}; // 11个C基本类型 + 联合体 + 结构体 + void 没了  C就这么多了
#pragma pack(1)
struct sub
{
	char name[8]; //纯属给人看的，没用处
	unsigned short level;
	unsigned char type;
	unsigned int low_pc;
	unsigned int high_pc;
	struct loc *loc;
	struct var *var;
	struct sub *next;
};

struct loc
{
	unsigned int begin;
	unsigned int end;
	unsigned char reg;// *data -0x70 对应上面的寄存器表   其实X86  -O0编译也就4、5两个值
	unsigned short base; // *(data+1)
	struct loc *next;
};

struct var
{
	char name[8];//给人看的。。。。
	unsigned int array; // 数组长度，0表示不是数组
	unsigned char points; //这个变量是几层指针，256够了把
	unsigned char type; //变量对应啥基本类型，给人看的。。。基本类型才13个啊
	unsigned int len;//类型长度 还是int吧  万一碰见变态的结构体呢，，，
	unsigned char loc_type; //位置类型  char就够用了
	unsigned int loc;//位置数据
	struct var *next;
};

struct info
{
	struct sub * sub_link;
	struct var * global;
};
#pragma pack()

struct info asd; ///给变量起名字什么的最烦人了！！@@￥%……*（

unsigned int cu_low_pc;
int is_new_cu;

static void get_addr(Dwarf_Attribute attr, Dwarf_Addr *val) {
	Dwarf_Error error = 0;
	int res;
	Dwarf_Addr uval = 0;
	res = dwarf_formaddr(attr, &uval, &error);
	if (res == DW_DLV_OK)
		*val = uval;
}

static struct sub *new_sub(char* name,unsigned int low,unsigned int high)
{
	struct sub *tmp_sub = NULL;
	tmp_sub = (struct sub*) malloc(sizeof(struct sub));
	if(tmp_sub)
	{
		memset(tmp_sub,0,sizeof(struct sub));
		if(name)
			memcpy(tmp_sub->name,name,strlen(name)>8?8:strlen(name));
		tmp_sub->low_pc = low;
		tmp_sub->high_pc = high;
		return tmp_sub;
	}
	else
		return NULL;
}

static void get_loc_list(Dwarf_Debug dbg, Dwarf_Die die, struct sub *sub)
{
	Dwarf_Unsigned offset=0;
	Dwarf_Error error = 0;
	Dwarf_Addr hipc_off;
	Dwarf_Addr lopc_off;
	Dwarf_Ptr data;
	Dwarf_Unsigned entry_len;
	Dwarf_Error err;
	int res;
	Dwarf_Unsigned next_entry;
	Dwarf_Attribute t_attr;
	struct loc *tmp=NULL,*pos;

	res = dwarf_attr(die, DW_AT_frame_base, &t_attr, &error);
	if (res == DW_DLV_OK)
	{
		res = dwarf_formudata(t_attr, &offset, &error);
		if (res == DW_DLV_OK)
			for(;;)
				{
					res = dwarf_get_loclist_entry(dbg,offset,&hipc_off,&lopc_off,&data,&entry_len,&next_entry,&err);
					if (res == DW_DLV_OK)
					{
						if(entry_len == 0 )
							break;
						if(entry_len > 1 )
						{
						tmp = (struct loc*) malloc(sizeof(struct loc));
						if(tmp)
						{
							memset(tmp,0,sizeof(struct loc));
							tmp ->begin = cu_low_pc + lopc_off;
							tmp ->end = cu_low_pc + hipc_off;
							tmp ->reg = (unsigned short)*(char*)data;
							if(entry_len == 2)
								tmp ->base = (unsigned short)*(char *)(data+1);
							else
								tmp ->base = (unsigned short)*(unsigned short *)(data+1);
							if(sub->loc)
							{
								pos = sub->loc;
								while(pos->next)
									pos = pos->next;
								pos->next = tmp;
							}
							else
								sub->loc = tmp;
						}
						else break;
						}
						offset = next_entry;
						continue;
					}
					else break;
				}
	}
}

static Dwarf_Die get_die(Dwarf_Debug dbg, Dwarf_Attribute attr, Dwarf_Half* tag) {//通过偏移量取die和对应的标签
	Dwarf_Error error = 0;
	int res;
	Dwarf_Off offset;
	Dwarf_Die typeDie = 0;
	res = dwarf_global_formref(attr, &offset, &error);
	if (res == DW_DLV_OK) {
		res = dwarf_offdie(dbg, offset, &typeDie, &error);
		if (res == DW_DLV_OK) {
			res = dwarf_tag(typeDie, tag, &error);
			if (res == DW_DLV_OK) {
				return typeDie;
			}
		}
	}
	return NULL ;
}

static void print_subprog(Dwarf_Debug dbg, Dwarf_Die die, struct sub **tmp_sub) {
	int res;
	Dwarf_Error error = 0;
	Dwarf_Addr lowpc = 0;
	Dwarf_Addr highpc = 0;
	Dwarf_Attribute t_attr;
	Dwarf_Half tag;
	Dwarf_Die org_die = NULL;
	char *name;
	res = dwarf_attr(die, DW_AT_abstract_origin, &t_attr, &error);
	if (res == DW_DLV_OK)
		org_die = get_die(dbg, t_attr, &tag);
	else org_die = die;
	res = dwarf_diename(org_die, &name, &error);
	if (res != DW_DLV_OK) {
		name = NULL;
	}
	res = dwarf_attr(die, DW_AT_low_pc, &t_attr, &error);
	if (res == DW_DLV_OK) // 获取DW_AT_low_pc
		get_addr(t_attr, &lowpc);
	res = dwarf_attr(die, DW_AT_high_pc, &t_attr, &error);
	if (res == DW_DLV_OK) // 获取DW_AT_high_pc
		get_addr(t_attr, &highpc);
	*tmp_sub = new_sub(name,(unsigned int)lowpc,(unsigned int)highpc);
	//目前只考虑32位的，unsigned int就行
	//}
}


static int get_array_length(Dwarf_Debug dbg, Dwarf_Die die, int *length) {//数组类型的长度。。
	int res;
	Dwarf_Error error;
	Dwarf_Die child;
	Dwarf_Attribute tmp;
	res = dwarf_child(die, &child, &error);
	*length = 1;
	Dwarf_Unsigned utmp;
	if (res == DW_DLV_OK) {
		while (1) {
			res = dwarf_attr(child, DW_AT_upper_bound, &tmp, &error);
			if (res == DW_DLV_OK) {
				res = dwarf_formudata(tmp, &utmp, &error);
				if (res != DW_DLV_OK)
					return DW_DLV_ERROR;
				else
					*length *= (utmp + 1);
			}
			res = dwarf_siblingof(dbg, child, &child, &error);
			if (res == DW_DLV_ERROR)
				return DW_DLV_ERROR;
			if (res == DW_DLV_NO_ENTRY)
				return DW_DLV_OK;
		}
	}
	return DW_DLV_ERROR;
}

static void get_type(Dwarf_Debug dbg, Dwarf_Attribute attr, unsigned int *arr, unsigned short *points, unsigned short *type, unsigned int *type_len) {
	char *name = 0;
	Dwarf_Half tag;
	Dwarf_Unsigned size;
	Dwarf_Error error = 0;
	Dwarf_Attribute t_attr;
	int res;
	int length;
	Dwarf_Die typeDie = get_die(dbg, attr, &tag);
	if (typeDie) {
		switch (tag) {//只处理了和标准C相关的类型
		case DW_TAG_subroutine_type://函数指针，对比没意义，就不管了
			*type = 255;
			break;
		case DW_TAG_typedef:
			goto next_type;
		case DW_TAG_const_type:
			goto next_type;
		case DW_TAG_pointer_type:
			*points += 1;
			goto next_type;
		case DW_TAG_volatile_type:
			next_type: res = dwarf_attr(typeDie, DW_AT_type, &t_attr, &error);
			if (res == DW_DLV_OK) {
				get_type(dbg, t_attr, arr, points, type, type_len);
			} else
				*type = 1;
			break;
		case DW_TAG_base_type:
			res = dwarf_diename(typeDie, &name, &error);
			if (res == DW_DLV_OK) {
				for(res = 0;res<sizeof(dwarf_base_type)/sizeof(char *);res++)
					if(!strncmp(name,dwarf_base_type[res],strlen(dwarf_base_type[res])))
					{
						*type = res+1;
						break;
					}
				res = dwarf_bytesize(typeDie, &size, &error);
				if (res == DW_DLV_OK) {
					*type_len = ((unsigned int)size);
				} else
					*type_len = 0;
			} else
				*type = 0;
			break;
		case DW_TAG_array_type:
			res = get_array_length(dbg, typeDie, &length);
			if (res == DW_DLV_OK)
				*arr = length;
			goto next_type;
		case DW_TAG_union_type:
			*type = sizeof(dwarf_base_type)/sizeof(char *) -1;
			goto get_size;
		case DW_TAG_structure_type:
			*type = sizeof(dwarf_base_type)/sizeof(char *);
			get_size: res = dwarf_bytesize(typeDie, &size, &error);
			if (res == DW_DLV_OK)
				*type_len = (unsigned int)size;
			else
				*type_len = 0;
			break;
		default:
			*type = 0;
			break;
		}
	}
}

static void get_location(Dwarf_Debug dbg, Dwarf_Attribute attr, unsigned int *loc_type, unsigned int *loc) {
	Dwarf_Error error = 0;
	int res;
	Dwarf_Locdesc *llbuf;
	Dwarf_Signed lcnt;
	res = dwarf_loclist(attr, &llbuf, &lcnt, &error);
	if (res == DW_DLV_OK) {
		*loc_type = (unsigned int) llbuf->ld_s->lr_atom;
		*loc = (unsigned int)llbuf->ld_s->lr_number;
		dwarf_dealloc(dbg, llbuf->ld_s, DW_DLA_LOC_BLOCK);
		dwarf_dealloc(dbg, llbuf, DW_DLA_LOCDESC);
	}
}

static void print_variable(Dwarf_Debug dbg, Dwarf_Die die, struct var ** tmp_loc) {
	int res;
	Dwarf_Error error = 0;
	Dwarf_Attribute t_attr;
	unsigned int arr, type_len, loc_type, loc;
	unsigned short points, type ;
	struct var * tmp =NULL ;
	Dwarf_Die org_die = NULL;
	Dwarf_Half tag;
	char *name;

	*tmp_loc = tmp;
	res = dwarf_attr(die, DW_AT_abstract_origin, &t_attr, &error);
	if (res == DW_DLV_OK)
		org_die = get_die(dbg, t_attr, &tag);
	else org_die = die;
	res = dwarf_diename(org_die, &name, &error);
	if (res != DW_DLV_OK) {
		name = NULL;
	}

	res = dwarf_attr(die, DW_AT_declaration, &t_attr, &error);
	if (res != DW_DLV_OK) { //若res == DW_DLV_OK，则是extern变量,没有位置信息，就不再保存
		loc_type = loc =0;
		arr = type_len = 0;
		points = type = 0;

		res = dwarf_attr(die, DW_AT_location, &t_attr, &error);
		if (res == DW_DLV_OK) //变量位置
			get_location(dbg, t_attr, &loc_type, &loc);

		if(loc_type == 0)
		{
			printf("no loc info !!\n");
			return;
		}

		res = dwarf_attr(org_die, DW_AT_type, &t_attr, &error);
		if (res == DW_DLV_OK) // 类型
			get_type(dbg, t_attr, &arr, &points, &type, &type_len);

		if(type == 0)
		{
			printf("error in get_type \n ");
			return;
		}
		if(type == 255)
		{
			printf("DW_TAG_subroutine_type !! \n");
			return;
		}
		tmp = (struct var*) malloc(sizeof(struct var));
		if (tmp)
		{
			memset(tmp,0,sizeof(struct var));
			if(name)
				memcpy(tmp->name,name,strlen(name)>8?8:strlen(name));
			tmp->array = arr;
			tmp->points = points;
			tmp->type = type;
			tmp->len = type_len;
			tmp->loc_type = loc_type;
			tmp->loc = loc;
			*tmp_loc = tmp;
		}
	}
}

static void get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,
		int in_level) {
	int res = DW_DLV_ERROR;
	Dwarf_Die cur_die = in_die;
	Dwarf_Die child = 0;
	Dwarf_Error error;
	Dwarf_Half tag;
	Dwarf_Attribute t_attr;
	Dwarf_Unsigned inline_tag;
	Dwarf_Die sib_die = 0;
	struct var *t_var , *p_var;
	struct sub *t_sub , *p_sub;
	struct todo
	{
		Dwarf_Die d;
		Dwarf_Half tag;
		struct todo *n;
	}*todo_list,*t_todo,*p_todo;

	todo_list = NULL;
	for(;;)
	{
		res = dwarf_tag(cur_die, &tag, &error); // 取标签
		if (res != DW_DLV_OK) {
			printf("Error in dwarf_tag , level %d \n", in_level);
			exit(1);
		}
		if(in_level == 1) //由于标准C不允许函数嵌套定义，SO ，1层只有全局变量和函数顶层
		{
			if(tag == DW_TAG_variable) //全局变量
			{
				print_variable(dbg, cur_die, &t_var);
				if(asd.global) //放进全局链结尾去
				{
					p_var = asd.global;
					while(p_var->next)
						p_var = p_var->next;
					p_var->next = t_var;
				}
				else
					asd.global = t_var;
			}
			if(tag == DW_TAG_subprogram) //函数，判断是否时inline 然后加进SUB链 然后进child取局部变量和包含的块和inlined
			{
				res = dwarf_attr(cur_die, DW_AT_inline, &t_attr, &error);
				if(res == DW_DLV_OK)
				{
					res = dwarf_formudata(t_attr, &inline_tag, &error);
					if(res != DW_DLV_OK)
						exit(1);
					if(inline_tag == DW_INL_inlined || inline_tag == DW_INL_declared_inlined)
						goto next_sib;//是inline函数  就出去继续循环  后面把非inline函数加进SUB
				}
				print_subprog(dbg, cur_die, &t_sub);
				if(t_sub)
				{
					if(is_new_cu)
					{
						is_new_cu = 0;
						cu_low_pc = t_sub->low_pc;
					}
					t_sub->level = in_level;
					t_sub->type = DW_TAG_subprogram;
					get_loc_list(dbg, cur_die, t_sub); //把loc信息添加进去
					if(asd.sub_link) //放进sub链结尾去
					{
						p_sub = asd.sub_link;
						while(p_sub->next)
							p_sub = p_sub->next;
						p_sub->next = t_sub;
					}
					else
						asd.sub_link = t_sub;

					res = dwarf_child(cur_die, &child, &error);
					if (res == DW_DLV_ERROR)
					{
						printf("Error in dwarf_child , level %d \n", in_level);
						exit(1);
					}
					if (res == DW_DLV_OK)
						get_die_and_siblings(dbg, child, in_level + 1);  //进入child  也就是>1的层
				}
			}
		}
		else if(in_level > 1) //大于1层的地方只有局部变量和块、inlined
		{
			if (tag == DW_TAG_formal_parameter || tag == DW_TAG_variable) //局部变量，挂到sub链最后一个的VAR指针上
			{
				print_variable(dbg, cur_die, &t_var);
				p_sub = asd.sub_link;
				while(p_sub->next)
					p_sub = p_sub->next;
				if(p_sub->var)
				{
					p_var = p_sub->var;
					while(p_var->next)
						p_var = p_var->next;
					p_var->next = t_var;
				}
				else
					p_sub->var = t_var;
			}
			if(tag == DW_TAG_lexical_block || tag == DW_TAG_inlined_subroutine) // 先放到todo_list里面在for外面单独处理
			{
				t_todo = (struct todo *) malloc(sizeof(struct todo));
				if(t_todo)
				{
					memset(t_todo,0,sizeof(struct todo));
					t_todo->d = cur_die;
					t_todo->tag = tag;
					if(todo_list)
					{
						p_todo = todo_list;
						while(p_todo->n)
							p_todo = p_todo->n;
						p_todo->n = t_todo;
					}
					else
						todo_list = t_todo;
				}
			}
		}
		else
		{//0层是变编译头信息，没啥用就直接进child
			res = dwarf_child(cur_die, &child, &error);
			if (res == DW_DLV_ERROR)
			{
				printf("Error in dwarf_child , level %d \n", in_level);
				exit(1);
			}
			if (res == DW_DLV_OK)
			{
				is_new_cu = 1;
				get_die_and_siblings(dbg, child, in_level + 1);
			}
		}
		next_sib:
		sib_die = 0;
		res = dwarf_siblingof(dbg, cur_die, &sib_die, &error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof , level %d \n", in_level);
			exit(1);
		}
		if (res == DW_DLV_NO_ENTRY)
			break;

		if (cur_die != in_die && in_level < 2)
			dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);

		cur_die = sib_die;
	}
	if(todo_list)
	{
		t_todo = p_todo = todo_list;
		todo_list = NULL;
		while(p_todo)
		{
			print_subprog(dbg, p_todo->d, &t_sub);
			if(t_sub)
			{
				t_sub->level = in_level;
				t_sub->type = p_todo->tag;
				if(asd.sub_link) //放进sub链结尾去
				{
					p_sub = asd.sub_link;
					while(p_sub->next)
						p_sub = p_sub->next;
					p_sub->next = t_sub;
				}
				else
					exit(1); //若在这里asd.sub_link依然为空，那么肯定是有错误了，因为只有在>1的时候才能进到这里

				res = dwarf_child(cur_die, &child, &error);
				if (res == DW_DLV_ERROR)
				{
					printf("Error in dwarf_child , level %d \n", in_level);
					exit(1);
				}
				if (res == DW_DLV_OK)
					get_die_and_siblings(dbg, child, in_level + 1);  //进入child
			}
			p_todo = p_todo->n;
			dwarf_dealloc(dbg, t_todo->d , DW_DLA_DIE);
			free(t_todo);
			t_todo = p_todo;
		}
	}
}


static void read_cu_list(Dwarf_Debug dbg) {//一次取出一个CU头
	Dwarf_Unsigned cu_header_length = 0;
	Dwarf_Half version_stamp = 0;
	Dwarf_Unsigned abbrev_offset = 0;
	Dwarf_Half address_size = 0;
	Dwarf_Unsigned next_cu_header = 0;
	Dwarf_Error error;

	for (;;) {
		Dwarf_Die no_die = 0;
		Dwarf_Die cu_die = 0;
		int res = DW_DLV_ERROR;
		res = dwarf_next_cu_header(dbg, &cu_header_length, &version_stamp,
				&abbrev_offset, &address_size, &next_cu_header, &error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_next_cu_header\n");
			exit(1);
		}
		if (res == DW_DLV_NO_ENTRY) {
			/* Done. */
			return;
		}
		/* The CU will have a single sibling, a cu_die. */
		res = dwarf_siblingof(dbg, no_die, &cu_die, &error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof on CU die \n");
			exit(1);
		}
		if (res == DW_DLV_NO_ENTRY) {
			/* Impossible case. */
			printf("no entry! in dwarf_siblingof on CU die \n");
			exit(1);
		}
		get_die_and_siblings(dbg, cu_die, 0);
		dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
	}
}

static void save_to_file(char *path, struct info *asd)
{
	FILE * f ,* fpos;
	char buf[255];
	static const int size[] =
	{
			sizeof(struct sub) - 3 * sizeof(void *),
			sizeof(struct loc) - sizeof(void *),
			sizeof(struct var) - sizeof(void *)
	};
	static const unsigned char tag[] =
	{
			0x00,0x01,0x02,0x03
	};
	struct sub *p_sub_1, *p_sub_2;
	struct loc *p_loc_1, *p_loc_2;
	struct var *p_var_1, *p_var_2;

	unsigned int pos;
	pos = 0;
	sprintf(buf,"%s.pos",path);
	f = fopen(path,"w");
	fpos = fopen(buf,"w");
	if(f)
	{
		p_var_1 = asd->global;
		while(p_var_1)
		{
			fwrite(tag+3, 1, 1, f);
			fwrite(p_var_1, size[2], 1, f);
			fwrite(tag, 1, 1, f);
			p_var_2 = p_var_1;
			p_var_1 = p_var_1->next;
			free(p_var_2);
			pos += size[2] + 2;
		}
		fwrite(&pos,4,1,fpos);
		asd->global = NULL;
		p_sub_1 = asd->sub_link;
		while(p_sub_1)
		{
			fwrite(tag+1, 1, 1, f);
			fwrite(p_sub_1, size[0], 1, f);
			fwrite(tag, 1, 1, f);
			fwrite(&p_sub_1->low_pc, sizeof(long), 1, fpos);
			fwrite(&p_sub_1->high_pc, sizeof(long), 1, fpos);
			fwrite(&pos,4,1,fpos);
			pos += size[0] + 2;
			p_loc_1 = p_sub_1->loc;
			while(p_loc_1)
			{
				fwrite(tag+2, 1, 1, f);
				fwrite(p_loc_1, size[1], 1, f);
				fwrite(tag, 1, 1, f);
				p_loc_2 = p_loc_1;
				p_loc_1 = p_loc_1->next;
				free(p_loc_2);
				pos += size[1] + 2;
			}
			p_var_1 = p_sub_1->var;
			while(p_var_1)
			{
				fwrite(tag+3, 1, 1, f);
				fwrite(p_var_1, size[2], 1, f);
				fwrite(tag, 1, 1, f);
				p_var_2 = p_var_1;
				p_var_1 = p_var_1->next;
				free(p_var_2);
				pos += size[2] + 2;
			}
			p_sub_2 = p_sub_1;
			p_sub_1 = p_sub_1->next;
			free(p_sub_2);
		}
		asd->sub_link = NULL;
		fclose(f);
		fclose(fpos);
	}
}
/*
static void read_from_file(char *path, struct info *asd)
{
	FILE * f;
	static const int size[] =
	{
			sizeof(struct sub) - 3 * sizeof(void *),
			sizeof(struct loc) - sizeof(void *),
			sizeof(struct var) - sizeof(void *)
	};

	struct sub *p_sub_1, *p_sub_2;
	struct loc *p_loc_1, *p_loc_2;
	struct var *p_var_1, *p_var_2;
	char t;
	int is_global = 1;

	f = fopen(path,"r");
	if(f)
	{
		while(!feof(f))
		{
			fread(&t, 1, 1, f);
			switch(t)
			{
			case 3:
				p_var_2 = (struct var *) malloc(sizeof(struct var));
				if(p_var_2)
				{
					memset(p_var_2, 0, sizeof(struct var));
					fread(p_var_2, size[2], 1, f);
					if(is_global)
					{
						if(asd->global)
						{
							p_var_1 = asd->global;
							while(p_var_1 ->next)
								p_var_1 = p_var_1->next;
							p_var_1->next = p_var_2;
						}
						else
							asd->global = p_var_2;
					}
					else
					{
						p_sub_1 = asd->sub_link;
						while(p_sub_1->next)
							p_sub_1 = p_sub_1->next;
						if(p_sub_1->var)
						{
							p_var_1 = p_sub_1->var;
							while(p_var_1 ->next)
								p_var_1 = p_var_1->next;
							p_var_1->next = p_var_2;
						}
						else
							p_sub_1->var = p_var_2;
					}
				}
				break;
			case 1:
				p_sub_2 = (struct sub *) malloc(sizeof(struct sub));
				if(p_sub_2)
				{
					is_global = 0;
					memset(p_sub_2, 0, sizeof(struct sub));
					fread(p_sub_2, size[0], 1, f);
					if(asd->sub_link)
					{
						p_sub_1 = asd->sub_link;
						while(p_sub_1->next)
							p_sub_1 = p_sub_1->next;
						p_sub_1->next = p_sub_2;
					}
					else
						asd->sub_link = p_sub_2;
				}
				break;
			case 2:
				p_loc_2 = (struct loc *) malloc(sizeof(struct loc));
				if(p_loc_2)
				{
					memset(p_loc_2, 0, sizeof(struct loc));
					fread(p_loc_2, size[1], 1, f);
					if(!asd->sub_link)
						exit(1);
					p_sub_1 = asd->sub_link;
					while(p_sub_1->next)
						p_sub_1 = p_sub_1->next;
					if(p_sub_1->loc)
					{
						p_loc_1 = p_sub_1->loc;
						while(p_loc_1 ->next)
							p_loc_1 = p_loc_1->next;
						p_loc_1->next = p_loc_2;
					}
					else
						p_sub_1->loc = p_loc_2;
				}
				break;
			}
			fread(&t, 1, 1, f);
			if(t)
				exit(1);
		}
	}
}

static void print_pos(char *path)
{
	FILE * f ,* fp;
	char buf[255], *b,name[9];
	unsigned int *bp;
	int s,sp,pos;
	sprintf(buf,"%s.pos",path);
	f = fopen(path,"r");
	fp = fopen(buf,"r");
	fseek(f,0,2);
	fseek(fp,0,2);
	s = ftell(f);
	sp = ftell(fp);
	b = (char *) malloc(s);
	bp = (unsigned int *) malloc(sp);
	fseek(f,0,0);
	fseek(fp,0,0);
	fread(b,s,1,f);
	fread(bp,s,1,fp);
	fclose(f);
	fclose(fp);
	printf("全局比啊两数据长度:%d\n",*bp);
	pos = 1;
	while(pos < sp/4)
	{
		printf("low: %08x\n",*(bp+pos));
		printf("hig: %08x\n",*(bp+pos+1));
		memset(name,0,9);
		memcpy(name,b+*(bp+pos+2)+1,8);
		printf("name:%s\n",name);
		pos+=3;
	}
}

static void print(struct info asd)
{
	struct sub *p_sub_1;
	struct loc *p_loc_1;
	struct var *p_var_1;
	char n[9];
	p_var_1 = asd.global;
	while(p_var_1)
	{
		memset(n,0,9);
		memcpy(n,p_var_1->name,8);
		printf("< 1 > 变量名: %s\n< 1 > ",n);
		if ( DW_OP_addr == p_var_1->loc_type)
			printf("直接地址: %x \n",(int) p_var_1->loc);
		else if(p_var_1->loc_type >= DW_OP_breg0 && p_var_1->loc_type <= DW_OP_breg31)
			printf("基址寻址: %d (%s)\n", ((int)p_var_1->loc), *(dwarf_regnames_i386+p_var_1->loc_type-DW_OP_breg0));
		else if(p_var_1->loc_type >= DW_OP_reg0 && p_var_1->loc_type <= DW_OP_reg31)
			printf("通用寄存器: reg%d \n",p_var_1->loc - DW_OP_reg0);
		else if(p_var_1->loc_type == DW_OP_fbreg)
			printf("loclist: %d \n",((int)p_var_1->loc));
		else
			printf("Unknow location \n");//不是C的不管丫的
		printf("< %d > 数组: %d\n",1,p_var_1->array);
		printf("< %d > 指针: %d\n",1,p_var_1->points);
		printf("< %d > 类型: %s\n",1,dwarf_base_type[p_var_1->type - 1]);
		printf("< 1 > 大小: %d\n\n",p_var_1->len);
		p_var_1 = p_var_1->next;
	}
	p_sub_1 = asd.sub_link;
	while(p_sub_1)
	{
		p_loc_1 = p_sub_1->loc;
		p_var_1 = p_sub_1->var;
		memset(n,0,9);
		memcpy(n,p_sub_1->name,8);
		printf("< %d >函数名: %s\n",p_sub_1->level,p_sub_1->type == DW_TAG_subprogram?n:"局部块");
		printf("< %d >low_pc : 0x%08x \n",p_sub_1->level,p_sub_1->low_pc);
		printf("< %d >high_pc : 0x%08x \n\n",p_sub_1->level,p_sub_1->high_pc);
		while(p_loc_1)
		{
			printf("< %d > loclist: 0x%08x -- 0x%08x : (%s) %d \n",p_sub_1->level,p_loc_1->begin,p_loc_1->end,*(dwarf_regnames_i386+(p_loc_1->reg>0x70?p_loc_1->reg-0x70:10)),p_loc_1->base);
			p_loc_1 = p_loc_1->next;
		}
		while(p_var_1)
		{
			memset(n,0,9);
			memcpy(n,p_var_1->name,8);
			printf("\n< %d > 变量名: %s\n< %d > ",p_sub_1->level+1,n,p_sub_1->level+1);
			if ( DW_OP_addr == p_var_1->loc_type)
				printf("直接地址: %x \n",(int) p_var_1->loc);
			else if(p_var_1->loc_type >= DW_OP_breg0 && p_var_1->loc_type <= DW_OP_breg31)
				printf("基址寻址: %d (%s)\n", ((int)p_var_1->loc), *(dwarf_regnames_i386+p_var_1->loc_type-DW_OP_breg0));
			else if(p_var_1->loc_type >= DW_OP_reg0 && p_var_1->loc_type <= DW_OP_reg31)
				printf("通用寄存器: reg%d \n",p_var_1->loc - DW_OP_reg0);
			else if(p_var_1->loc_type == DW_OP_fbreg)
				printf("loclist: %d \n",((int)p_var_1->loc));
			else
				printf("Unknow location \n");//不是C的不管丫的
			printf("< %d > 数组: %d\n",p_sub_1->level+1,p_var_1->array);
			printf("< %d > 指针: %d\n",p_sub_1->level+1,p_var_1->points);
			printf("< %d > 类型: %s\n",p_sub_1->level+1,dwarf_base_type[p_var_1->type - 1]);
			printf("< %d > 大小: %d\n\n",p_sub_1->level+1,p_var_1->len);
			p_var_1 = p_var_1->next;
		}
		p_sub_1 = p_sub_1->next;
	}
}
*/
int main(int argc, char **argv) {//初始化dbg
	Dwarf_Debug dbg = 0;
	int fd = -1;
	const char *filepath = "<stdin>";
	char p[255];
	int res = DW_DLV_ERROR;
	Dwarf_Error error;
	Dwarf_Handler errhand = 0;
	Dwarf_Ptr errarg = 0;

	if (argc < 2)
		fd = 0;
	else
	{
		filepath = argv[1];
		fd = open(filepath, O_RDONLY);
	}
	if (fd <= 0)
	{
		printf("Failure attempting to open \"%s\"\n", filepath);
		exit(1);
	}
	res = dwarf_init(fd, DW_DLC_READ, errhand, errarg, &dbg, &error);
	if (res != DW_DLV_OK) {
		printf("Giving up, cannot do DWARF processing\n");
		exit(1);
	}
	read_cu_list(dbg);
	res = dwarf_finish(dbg, &error);
	if (res != DW_DLV_OK) {
		printf("dwarf_finish failed!\n");
	}
	close(fd);
	sprintf(p,"%s.dwarf",filepath);
	if(asd.global || asd.sub_link)
		save_to_file(p,&asd);
	//read_from_file(p,&asd);
	//print(asd);
	//print_pos(p);
	printf("end \n");
	return 0;
}
