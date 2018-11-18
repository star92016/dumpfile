#ifndef _DUMP_FILE_INNER_H_
#define _DUMP_FILE_INNER_H_

#include"dumpfile.h"

#define MODE_READ 1
#define MODE_WRITE 2


typedef struct dumpreloc{
	int len;
	size_t offset[0];
}dumpreloc;

typedef struct dumpitem{
	char *name;
	void *mem;
	size_t len;
	dumpreloc* reloc;
	
	struct dumpitem* next;
	struct dumpitem* last;
	
	struct dumpitem* sort_next;
}dumpitem;

struct dumpfile_disk;
struct dumpfile{
    FILE* f;
    int mode;
	int isfinished;
	dumpitem* head;
	dumpitem* tail;
	dumpitem* sort_head;
	struct dumpfile_disk* disk;
};

typedef struct dumpfile_disk_item{
	unsigned int name;
	unsigned int mem;
	unsigned int len;
	unsigned int reloc;
	unsigned int reloc_len;
}dumpfile_disk_item;

typedef struct dumpfile_disk_str{
	unsigned int offset;
}dumpfile_disk_str;

typedef struct dumpfile_disk_reloc{
	unsigned int offset;
	unsigned int item_index;
	unsigned int item_offset;
}dumpfile_disk_reloc;

typedef struct dumpfile_disk{
	char majic[4];//DUMP
	unsigned short version_major;
	unsigned short version_minor;
	
	void** map;//size==item_count
	
	unsigned int item_count;
	dumpfile_disk_item* items;
	
	unsigned int str_count;
	dumpfile_disk_str* strings;
	
	unsigned int str_size;
	char* str_data;
	
	unsigned int reloc_count;
	dumpfile_disk_reloc* relocs;
} dumpfile_disk;

#endif
