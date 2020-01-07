#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>

//定义的函数
void accept_request(int);
int get_line(int,char*,int);
void error_request(int);
void execute_cgi(int,char*,char*,char*);
void serve_file(int, char *);
void cat(int, FILE *);
void not_found(int);
void bad_request(int);
void cannot_execute(int);
void error_die(const char *);
void headers(int, const char *);
int startup(u_short *);

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
    char_num=get_line(client,buf,sizeof(buf));
    //处理接收数据
    i=0;
    j=0;
    //取出第一个单词，一般为GET或POST
    while(!(' '==buf[j])&&(i<sizeof(method)-1))
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
    while((' '==buf[j])&&i<sizeof(buf)-1)
    {
       j++;
    }
    while(!(' '==buf[j])&&i<sizeof(buf)-1)
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
	    char_num=get_line(client,buf,sizeof(buf));
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

//表明该请求方法不被支持的函数
void error_request(int client )
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<html><head><title>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    sprintf(buf, "</title></head>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    sprintf(buf, "<body><p>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING); 
    sprintf(buf, "</body></html>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
 
 }

//cgi程序的处理
void  execute_cgi(int client, const char *path, const char *method, char *string)
{
    char buf[1024];
    char input[2];
    char output[2];
    char c;
    pid_t pid;
    int stus;
    int i;
    int char_num=1;
    content_lenght=-1;
   //如果是post请求
    if(strcasecmp(method,"POST")==0)
    {
       //获取下一行请求
       char_num=get_line(client,buf,sizeof(buf));
       while((char_num>0)&&strcmp("\n",buf))
       {
	    buf[15]='\0';
	    if(strcasecmp(buf,"Content-Length:")==0)
	    {
		content_lenght=atoi(&buf[16]);
	    }
	    char_num=get_line(client,buf,sizeof(buf));
       }
       //如果没有找到content_lenght
       if(content_lenght==-1)
       {
          bad_request(client);
	  return;
       }

    }
    //正确，状态玛为200
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    //建立管道
    if(pipe(output)<0)
    {
	//错误处理
        cannot_execute(client);
        return;
    }
    if(pipe(input)<0)
    {
        cannot_execute(client);
        return;
    }
    if((pid==fork())<0)
    {
        cannot_execute(client);
        return;
    }
    //创建子进程
    if(pid==0)
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];
	//把 STDOUT 重定向到output的写入端 
	dup2(output[1],1);
	// 把 STDIN 重定向到input的读取端
	dup2(input[0]，0);
        //关闭input的写入端 和output的读取端 
	close(output[0]);
	close(input[1]);
	//设置请求方法的环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
	//GET方法
        if (strcasecmp(method, "GET") == 0) 
	{
            //设置string的环境变量
	    sprintf(query_env, "QUERY_STRING=%s",string);
            putenv(query_env);
	}
	//POST方法
	else
       	{  
            //设置 content_lenght 的环境变量
            sprintf(length_env, "CONTENT_LENGTH=%d", content_lenght);
            putenv(length_env);
        }
        //用execl运行cgi程序
        execl(path, path, NULL);
        exit(0);

    }
    else
    {  
        //关闭input的读取端 和output的写入端
        close(output[1]);
        close(input[0]);
        if (strcasecmp(method, "POST") == 0)
	{
            //接收 POST 过来的数据
           for (i = 0; i < content_lenght; i++) 
	   {
               recv(client, &c, 1, 0);
               //把 POST 数据写入input，现在重定向到 STDIN
                write(input[1], &c, 1);
	   }
	}
        //读取output的管道输出到客户端，该管道输入是 STDOUT
        while (read(output[0], &c, 1) > 0)
	{
          send(client, &c, 1, 0);
	}
        //关闭管道
        close(output[0]);
        close(input[1]);
        //等待子进程
        waitpid(pid, &stus, 0);
    }
}

//调用cat函数把服务器文件返回给浏览器
void serve_file(int client,char *filename)
{
    FILE *resource = NULL;
    int char_num = 1;
    char buf[1024];

    //读取并丢弃 header
    while ((char_num > 0) && strcmp("\n", buf))
    {
        char_num = get_line(client, buf, sizeof(buf));
    }
    // 打开 sever 的文件
    resource = fopen(filename, "r");
    if (resource == NULL)
    {
        not_found(client);
    }
    else
    {
        //写 HTTP header 
        headers(client, filename);
        //复制文件
        cat(client, resource);
    }
    fclose(resource);
}

//读取服务器上某个文件写到 socket 套接字
void cat(int client, FILE *resource)
{
   char buf[1024];

   //读取文件中的所有数据写到 socket
   fgets(buf, sizeof(buf), resource);
   while (!feof(resource))
   {
      send(client, buf, strlen(buf), 0);
      fgets(buf, sizeof(buf), resource);
   }
}

//处理找不到请求文件的情况
void not_found(int client)
{
   char buf[1024];
   //404页面
   sprintf(buf, "HTTP/1.0 404 Not Found!\r\n");
   send(client, buf, strlen(buf), 0);
   //服务器信息
   sprintf(buf, SERVER_STRING);
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "Content-Type: text/html\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "<html><title>Not Found</title>\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "<body><p>The server could not fulfill\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "your request because the resource specified\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "is unavailable or nonexistent.\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "</body></html>\r\n");
   send(client, buf, strlen(buf), 0);
}

//处理客户端错误请求的函数
void bad_request(int client)
{
    char buf[1024];

    //回应客户端错误的 HTTP 请求 
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

//处理发生在执行 cgi 程序时出现的错误
void cannot_execute(int client)
{
    char buf[1024];

    // 回应客户端 cgi 无法执行
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

//把错误信息写到 perror 并退出
void error_die(const char *sc)
{
    //出错信息处理
    perror(sc);
    exit(1);
}

//把 HTTP 响应的头部写到套接字
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;

    //正常的 HTTP header
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    //服务器信息
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

//初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    //1、建立 socket 
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
    {
       error_die("socket");
    }
    //初始化
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    //2、绑定
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
       error_die("bind");
    }
    //如果当前指定端口是 0，则动态随机分配一个端口
    if (*port == 0)
    {
       int namelen = sizeof(name);
       if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
       {
          error_die("getsockname");
       }
       *port = ntohs(name.sin_port);
    }
    // 3、开始监听
    if (listen(httpd, 5) < 0)
    {
       error_die("listen");
    }
    //返回 socket id
    return(httpd);
}

int main(void)
{
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);
    pthread_t newthread;

    //在对应端口建立 httpd 服务
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        //套接字收到客户端连接请求
        client_sock = accept(server_sock,(struct sockaddr *)&client_name,&client_name_len);
        if (client_sock == -1)
	{
           error_die("accept");
	}
        //派生新线程用 accept_request 函数处理新请求
        if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)
	{
           perror("pthread_create");
	}
    }
    //关闭
    close(server_sock);

    return 0;
}
