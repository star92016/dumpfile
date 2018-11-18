#include"dumpfile_inner.h"

static void dump_file_seek(dumpfile *df,size_t pos){
	fseek(df->f,pos,SEEK_SET);
}

static void* dump_file_read(dumpfile *df,void *buf,size_t len){
	if(1!=fread(buf,len,1,df->f))return NULL;
	return buf;
}

static void dump_file_write(dumpfile *df,void *buf,size_t len){
	fwrite(buf,len,1,df->f);
}

dumpfile* dump_file_init(const char*filename,const char* mode){
    dumpfile* df=malloc(sizeof(dumpfile));
	df->f=NULL;
	df->head=NULL;
	df->tail=NULL;
	df->sort_head=NULL;
	df->isfinished=0;
	df->disk=NULL;
    if(mode){
		if(mode[0]=='r'){
			df->f=fopen(filename,"rb");
			df->mode=MODE_READ;
			df->isfinished=1;
		}else if(mode[0]=='w'){
			df->f=fopen(filename,"wb");
			df->mode=MODE_WRITE;
		}
	}
	
    if(!df->f){
        free(df);
        df=NULL;
    }
	#if 0
	if(df->mode==MODE_READ){
		char buf[8];
		char token[8]="DUMP";
		*((short*)(token+4))=1;
		*((short*)(token+2))=0;
		int flag=0;
		if(dump_file_read(df,buf,sizeof(buf))){
			if(memcmp(buf,token,sizeof(buf))==0){
				flag=1;
			}
		}
		if(!flag){
			fclose(df->f);
			free(df);
			df=NULL;
		}
	}
	#endif
    return df;
}


static void disk_add_str(dumpfile_disk*dk,const char*str){
	if(!str) return;
	if(dk->str_data){
		char *p=dk->str_data;
		while(p<dk->str_data+dk->str_size){
			if(strcmp(p,str)==0){
				return;
			}
			p=p+strlen(p)+1;
		}
	}
	
	if(dk->str_data){
		dk->str_data=realloc(dk->str_data,dk->str_size+strlen(str)+1);
	}else{
		dk->str_data=malloc(strlen(str)+1);
	}
	strcpy(dk->str_data+dk->str_size,str);
	
	
	dk->str_count++;
	dk->strings=realloc(dk->strings,sizeof(dumpfile_disk_str)*dk->str_count);
	dk->strings[dk->str_count-1].offset=dk->str_size;
	
	dk->str_size=dk->str_size+strlen(str)+1;
}

static int disk_find_str(dumpfile_disk*dk,const char*str){
	if(!str) return 0;
	int i;
	for(i=1;i<dk->str_count;i++){
		if(strcmp(str,dk->str_data+dk->strings[i].offset)==0)return i;
	}
	return 0;
}

typedef struct filedump_mem_map{
	void* mem;
	size_t len;
	int item_index;
	struct filedump_mem_map *next;
} filedump_mem_map;
//映射内存地址与文件index关系
static void add_mem_map(filedump_mem_map* head,void* mem,size_t len,int item_index){
	filedump_mem_map* node=head->next,*last=NULL;
	while(node){
		if(mem<node->mem)break;
		last=node;
		node=node->next;
	}
	filedump_mem_map* newnode=malloc(sizeof(filedump_mem_map));
	newnode->mem=mem;
	newnode->len=len;
	newnode->item_index=item_index;
	newnode->next=node;
	if(last==NULL) head->next=newnode;
	else last->next=newnode;
}
static void find_mem_map(filedump_mem_map* head,void*mem,int *item_index,int * offset){
	*item_index=0;
	*offset=0;
	if(mem==NULL){
		return;
	}
	filedump_mem_map* node=head->next;
	while(node){
		if(mem>=node->mem && mem< node->mem + node->len){
			*item_index=node->item_index;
			*offset=mem-node->mem;
			break;
		}
		node=node->next;
	}
	return;
}
typedef struct filedump_mem_map_wait{
	void* mem;
	dumpfile_disk_reloc*reloc;
	struct filedump_mem_map_wait *next;
} filedump_mem_map_wait;
//延时修改文件 重定位表
static void add_mem_wait_map(filedump_mem_map_wait* head,void* mem,dumpfile_disk_reloc* reloc){
	filedump_mem_map_wait* wait=malloc(sizeof(filedump_mem_map_wait));
	wait->mem=mem;
	wait->reloc=reloc;
	wait->next=head->next;
	head->next=wait;
}

static void dump_file_sync(dumpfile* df){
	if(!df->isfinished){
		df->isfinished=1;
		df->disk=malloc(sizeof(dumpfile_disk));
		
		
		dumpfile_disk*dk=df->disk;
		dk->map=NULL;
		
		strcpy(dk->majic,"DUMP");
		dk->version_major=1;
		dk->version_minor=0;
		
		dk->str_data=NULL;
		dk->str_size=0;
		
		//min number
		dk->item_count=1;
		dk->reloc_count=1;
		dk->str_count=1;
		
		dk->strings=malloc(sizeof(dumpfile_disk_str));
		dk->strings[0].offset=0;
		dumpitem* di=df->head;
		while(di){
			dk->item_count++;
			disk_add_str(dk,di->name);
			if(di->reloc)
				dk->reloc_count+=di->reloc->len;
			
			di=di->next;
		}
		dk->items=malloc(sizeof(dumpfile_disk_item)*dk->item_count);
		dk->relocs=malloc(sizeof(dumpfile_disk_reloc)*dk->reloc_count);
		
		dk->items[0].name=0;
		dk->items[0].mem=0;
		dk->items[0].len=0;
		dk->items[0].reloc=0;
		dk->items[0].reloc_len=0;
		
		dk->relocs[0].offset=0;
		dk->relocs[0].item_index=0;
		dk->relocs[0].item_offset=0;
		
		unsigned int total_len=8;
		
		total_len+=4;
		total_len+=(dk->item_count*sizeof(dumpfile_disk_item));
		
		total_len+=4;
		total_len+=(dk->str_count*sizeof(dumpfile_disk_str));
		
		total_len+=4;
		total_len+=dk->str_size;
		
		total_len+=4;
		total_len+=(dk->reloc_count*sizeof(dumpfile_disk_reloc));
		
		di=df->tail;
		int reloc_index=1;
		int item_index=1;
		
		filedump_mem_map mem_map={.next=NULL};
		filedump_mem_map_wait mem_wait={.next=NULL};
		
		while(di){
			dk->items[item_index].name=disk_find_str(dk,di->name);
			dk->items[item_index].mem=total_len;
			dk->items[item_index].len=di->len;
			dk->items[item_index].reloc=0;
			dk->items[item_index].reloc_len=0;
			
			add_mem_map(&mem_map,di->mem,di->len,item_index);
			
			total_len+=di->len;
			
			if(di->reloc){
				dk->items[item_index].reloc=reloc_index;
				dk->items[item_index].reloc_len=di->reloc->len;
				int i;
				for(i=0;i<di->reloc->len;i++){
					dk->relocs[reloc_index].offset=di->reloc->offset[i];
					dk->relocs[reloc_index].item_index=0;
					dk->relocs[reloc_index].item_offset=0;
					
					add_mem_wait_map(&mem_wait,*(void**)(di->mem+di->reloc->offset[i]),
						&dk->relocs[reloc_index]);
					reloc_index++;
				}
			}
			
			item_index++;
			
			di=di->last;
		}
		
		//now relocate it
		filedump_mem_map_wait*wait=(&mem_wait)->next;
		while(wait){
			//find_mem_map(filedump_mem_map* head,void*mem,int *item_index,int * offset)
			find_mem_map(&mem_map,wait->mem,&wait->reloc->item_index,
				&wait->reloc->item_offset);
			filedump_mem_map_wait*waitold=wait;
			wait=wait->next;
			free(waitold);
		}
		filedump_mem_map* map_node=(&mem_map)->next;
		while(map_node){
			filedump_mem_map* map_node_old=map_node;
			map_node=map_node->next;
			free(map_node_old);
		}
		
		//write
		
		dump_file_write(df,&dk->majic,4);
		dump_file_write(df,&dk->version_major,2);
		dump_file_write(df,&dk->version_minor,2);
		dump_file_write(df,&dk->item_count,4);
		dump_file_write(df,dk->items,dk->item_count*sizeof(dumpfile_disk_item));
		
		dump_file_write(df,&dk->str_count,4);
		dump_file_write(df,dk->strings,dk->str_count*sizeof(dumpfile_disk_str));
		
		dump_file_write(df,&dk->str_size,4);
		dump_file_write(df,dk->str_data,dk->str_size);
		
		
		dump_file_write(df,&dk->reloc_count,4);
		dump_file_write(df,dk->relocs,dk->reloc_count*sizeof(dumpfile_disk_reloc));
		
		di=df->tail;
		while(di){
			dump_file_write(df,di->mem,di->len);
			di=di->last;
		}
		fflush(df->f);
	}
}

static void clean_file_disk(dumpfile* df){
	dumpfile_disk*dk=df->disk;
	if(dk){
		if(dk->items) free(dk->items);
		if(dk->strings) free(dk->strings);
		if(dk->str_data) free(dk->str_data);
		if(dk->relocs) free(dk->relocs);
		
		if(dk->map)
		{
			#if 0
			int i;
			for(i=0;i<dk->item_count;i++){
				if(dk->map[i]) free(dk->map[i]);
			}
			#endif
			free(dk->map);
		} 
		
		free(dk);
		df->disk=NULL;
	}
}

static void clean_filedump(dumpfile* df){
	dumpitem* di=df->head;
	while(di){
		if(di->reloc) free(di->reloc);
		dumpitem* diold=di;
		di=di->next;
		free(diold);
	}
	df->head=NULL;
	df->tail=NULL;
	df->sort_head=NULL;
}

//close and write data to file
void dump_file_close(dumpfile* df){
	dump_file_sync(df);
	clean_file_disk(df);
	clean_filedump(df);
    if( df->f ) fclose(df->f);
    free(df);
}

void dump_file_add(dumpfile* df,void *mem,size_t len,size_t *offset){
	dump_file_add_name(df,NULL,mem,len,offset);
}
void dump_file_add_name(dumpfile* df,const char* name,void *mem,size_t len,size_t *offset){
	if(df->isfinished)return;
	
	dumpitem* di=df->sort_head;
	while(di){
		if(mem>=di->mem && mem< di->mem + di->len){
			return;
		}
		di=di->sort_next;
	}
	
	di=malloc(sizeof(dumpitem));
	di->name=(char*)name;
	di->mem=mem;
	di->len=len;
	di->next=df->head;
	di->last=NULL;
	di->reloc=NULL;
	if(df->tail==NULL)df->tail=di;
	if(df->head)df->head->last=di;
	df->head=di;
	
	dumpitem* sort_di=df->sort_head,*sort_last=NULL;
	while(sort_di){
		if(di->mem < sort_di->mem) break;
		sort_last=sort_di;
		sort_di=sort_di->sort_next;
	}
	di->sort_next=sort_di;
	if(sort_last==NULL) df->sort_head=di;
	else sort_last->sort_next=di;
	
	int l=0;
	while(offset[l++]!=-1);
	l--;
	
	if(l>0){
		dumpreloc* reloc=malloc(sizeof(dumpreloc)+l*sizeof(size_t));
		reloc->len=l;
		int i;
		for(i=0;i<l;i++){
			reloc->offset[i]=offset[i];
		}
		di->reloc=reloc;
	}
}
//load first added,if exit relocate pos,load them too


void* dump_file_load(dumpfile* df){
	return dump_file_load_name(df,NULL);
}

static void disk_load_item(dumpfile*df,int pos){
	
	dumpfile_disk* dk=df->disk;
	if(pos==0||dk->map[pos])return;
	
	dump_file_seek(df,dk->items[pos].mem);
	dk->map[pos]=malloc(dk->items[pos].len);
	dump_file_read(df,dk->map[pos],dk->items[pos].len);
	
	if(dk->items[pos].reloc_len>0){
		int i;
		for(i=0;i<dk->items[pos].reloc_len;i++){
			int reloc_i=dk->items[pos].reloc+i;
			
			disk_load_item(df,dk->relocs[reloc_i].item_index);
			if(dk->relocs[reloc_i].item_index==0) *(void**)(dk->map[pos]+dk->relocs[reloc_i].offset)=NULL;
			else
			*(void**)(dk->map[pos]+dk->relocs[reloc_i].offset)=dk->map[dk->relocs[reloc_i].item_index]+dk->relocs[reloc_i].item_offset;
				
		}
	}
}
#ifdef DEBUG
#define head_print(...) printf(__VA_ARGS__)
#else
#define head_print(...)
#endif
static void disk_load_head(dumpfile*df){
	dumpfile_disk* dk=malloc(sizeof(dumpfile_disk));
	df->disk=dk;
	dk->map=NULL;
	dk->items=NULL;
	dk->item_count=0;
	dk->strings=NULL;
	dk->str_count=0;
	dk->str_data=NULL;
	dk->str_size=0;
	dk->relocs=NULL;
	dk->reloc_count=0;
	
	dump_file_seek(df,0);
	dump_file_read(df,&dk->majic,4);
	dump_file_read(df,&dk->version_major,2);
	dump_file_read(df,&dk->version_minor,2);
	int i;
	
	if(*((int*)dk->majic)==*((int*)"DUMP") && dk->version_major==1 &&
		dk->version_minor==0){
		//ok
		head_print("DUMP\n%d.%d\n",dk->version_major,dk->version_minor);
		
		dump_file_read(df,&dk->item_count,4);
		head_print("item_count=%d\n",dk->item_count);
		dk->map=malloc(sizeof(void*)*dk->item_count);
		memset(dk->map,0,sizeof(void*)*dk->item_count);
		dk->items=malloc(sizeof(dumpfile_disk_item)*dk->item_count);
		
		dump_file_read(df,dk->items,sizeof(dumpfile_disk_item)*dk->item_count);
		for(i=0;i<dk->item_count;i++){
			head_print("[%d] name=%d mem=%d len=%d reloc=%d reloc_len=%d\n",
				i,dk->items[i].name,dk->items[i].mem,dk->items[i].len,
				dk->items[i].reloc,dk->items[i].reloc_len);
		}
		dump_file_read(df,&dk->str_count,4);
		head_print("str_count=%d\n",dk->str_count);
		dk->strings=malloc(sizeof(dumpfile_disk_str)*dk->str_count);
		dump_file_read(df,dk->strings,sizeof(dumpfile_disk_str)*dk->str_count);
		
		for(i=0;i<dk->str_count;i++){
			head_print("[%d] offset=%d\n",i,dk->strings[i].offset);
		}
		dump_file_read(df,&dk->str_size,4);
		head_print("str_size=%d\n",dk->str_size);
		dk->str_data=malloc(dk->str_size);
		dump_file_read(df,dk->str_data,dk->str_size);
		
		dump_file_read(df,&dk->reloc_count,4);
		head_print("reloc_count=%d\n",dk->reloc_count);
		dk->relocs=malloc(sizeof(dumpfile_disk_reloc)*dk->reloc_count);
		dump_file_read(df,dk->relocs,sizeof(dumpfile_disk_reloc)*dk->reloc_count);
		for(i=0;i<dk->reloc_count;i++){
			head_print("[%d] offset=%d item_index=%d item_offset=%d\n",
				i,dk->relocs[i].offset,dk->relocs[i].item_index,
				dk->relocs[i].item_offset);
		}
	}
	
}
#undef head_print
void* dump_file_load_name(dumpfile* df,const char* tagname){
	if(df->mode==MODE_WRITE) return NULL;
	
	if(df->disk==NULL){
		disk_load_head(df);
	}
	dumpfile_disk* dk=df->disk;
	int stroff = disk_find_str(dk,tagname);
	int i;
	
	for(i=1;i<dk->item_count;i++){
		if(dk->items[i].name==stroff){
			disk_load_item(df,i);
			return dk->map[i];
		}
	}
	
	return NULL;
} 

#define TEST_DUMP
#ifdef TEST_DUMP
typedef struct list{
    struct list* next;
    int data;
}list;

list _data[5]={
{_data+1,0},
{_data+2,1},
{_data+3,2},
{_data+4,3},
{NULL,4}
};

list* head=_data;

void show(list* h){
    list* node=h;
    while(node){
        printf("%d ",node->data);
        node=node->next;
    }
}

int main(){
    #ifdef TEST_DUMP_READ
    dumpfile*df= dump_file_init("hello","r");
    if(df==NULL){
    	printf("文件打开失败");
    	return 0;
    }
    list*tmp=dump_file_load_name(df,"namea");
    if(tmp){
    	show(tmp);
    }
    #else
    show(head);
    dumpfile*df= dump_file_init("hello","w");
    if(df==NULL){
    	printf("文件打开失败");
    	return 0;
    }
    
	char*names[5]={"namea","nameb","namec","nameb","namea"};
	size_t offs[2]={offset_of_struct(next,list),-1};
    
    int i;
    for(i=0;i<4;i++){
    	dump_file_add_name(df,names[i],_data+i,sizeof(list),offs);
    }
    dump_file_close(df);
    #endif
    return 0;
}
#endif

