#ifndef _DUMP_FILE_H_
#define _DUMP_FILE_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

//计算b.a 在b的偏移量
#define offset_of_struct(a,b) ((size_t)(&((b *)NULL)->a))
struct dumpfile;
typedef struct dumpfile dumpfile;
//初始化 filename(文件名) mode("r"/"w")
dumpfile* dump_file_init(const char*filename,const char* mode);
//关闭
void dump_file_close(dumpfile* df);
//当mode=="w"可调用,添加转储对象 mem内存开始地址 len长度
//offset 重定位偏移量数组，以-1结束 
//{4,-1}表示 *((void**)mem+4) 是一个指针,指针指向的内存也转储了
void dump_file_add(dumpfile* df,void *mem,size_t len,size_t *offset);
//同上,添加命名转储对象
void dump_file_add_name(dumpfile* df,const char* name,void *mem,size_t len,size_t *offset);
//当mode=="r"可调用,加载第一次加入的对象
//属于内存指针的区域会自动完成重定位
//返回的内存区域在dump_file_close后不会被释放，可长期持有
void* dump_file_load(dumpfile* df);
//同上,加载第一次加入的命名对象
void* dump_file_load_name(dumpfile* df,const char*name);

#endif
