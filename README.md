# dumpfile
## 这是什么
这是一个内存转储的库，可以编译成.so、.a文件供其他程序调用。在需要长时间计算的数据结构中，为避免再次运行程序等待太长时间，可考虑使用它。对计算出的数据转储文件，节省下次运行计算时间。
## 编译
gcc -shared -fPIC -o libdumpfile.so dumpfile.c
## 复制
cp dumpfile.h /usr/include
cp libdumpfile.so /usr/lib
## 写main.c代码
```c
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<dumpfile.h>
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
	#define TEST_DUMP_READ
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
```
## 编译
gcc main.c -ldumpfile
