#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/sockets.h>
#include<netinet.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>

//定义的函数
void accept_request(int);
int getline(int,char*,int);
void error_request(int);
void  execute_cgi(int,char*,char*,char*);
void  serve_file(int, char *);




//处理从套接字上监听的一个http请求
void accept_request(int client)
{ 
    char buf[1024];
    int char_num;
    char method[255];//用于存储方法的数组
    char uri[255];//用于存储uri的数组
    char path[512];//用于存储根路径的数组
    size_t i,j;
    struct stat st;
    int flag=0;//用于标识的变量
    char *string=NULL;

    //获取请求信息的第一行
    char_num=getline(client,buf,sizeof(buf));
    //处理接收数据
    i=0;
    j=0;
    //取出第一个单词，一般为GET或POST
    while(!(''==buf[j])&&(i<sizeof(method)-1))
    {
	method[i]==buf[j];
	i++;
	j++;
    }
    //结束符
    method[i]='\0';
    //如果不是get方法或post方法，则无法处理
    if(strcasecmp(method,"GET")&&strcasecmp(method,"POST"))
    {
	 error_request(client);
	 return;
    }
    //post：开启cgi
    if (strcasecmp(method, "POST") == 0)
    {
	flag=1;
    }

    //提取第二个单词
    //中间以空格分开字段
    while((''==buf[j])&&i<sizeof(buf)-1)
    {
       j++;
    }
    while(!(''==buf[j])&&i<sizeof(buf)-1)
    {
       uri[i]=buf[j];
       i++;
       j++;
    }
    uri[i]='\0';
    //处理get方法
    if(strcasecmp(method,"GET")==0)
    {
       string=uri;
       while((*string!='?')&&(*string!='\0'))
       {
	  string++;
       }
       if(*string=='?')
       {
	  //开启cgi
	  flag=1;
	  *string='\0';
	  string++;
       }
    }
    //格式化uri到path数组中
    sprintf(path,"hdocs%s",uri);
    //根据路径查找文件
    //如果找不到
    if(stat(path,&st)==-1)
    {
       //丢弃所有headers信息
       while((char_num>0)&&(strcmp("\n",buf)))
       {
	    //获取下一行请求信息   
	    char_num=getline(client,buf,sizeof(buf));
            //向客户端发送找不到文件
	    not_found(client);
       }
    }
    //如果找到
    else
    {
	//如果是cgi
	if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
	{
	    flag=1;
	    //执行cgi
	    execute_cgi(client, path, method,string);
	}
        //否则直接返回服务器文件
	serve_file(client, path);
    }
}

//获取下一行请求的函数
int getline(int sock,char *buf,int size)
{
    int n,i=0;
    char c='\0';
    while((i<size-1)&&(c!='\n'))
    {
       //一次仅仅接收一个字节
       n=recv(sock,&c,1,0);
       if(n>0)
       {
	  if(c=='\r')
	  {
            //下一次读取依然可以获得这次读取的内容
	     n=recv(sock,&c,1,MSG_PEEK);
	 
	    //如果是换行，则把它吸收
	     if ((n > 0) && (c == '\n'))
             {
                recv(sock, &c, 1, 0);
	     }
	     else
                c='\n';
	  }
          //存储到缓存区
          buf[i]=c;
          i++;
       }
       else
	  c = '\n';
    }
    buf[i] = '\0';
    //返回数组大小
    return (i);
}

      


       



        


