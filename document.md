### A. 基本要求：

\- 支持GET、HEAD和POST三种请求方法

\- 支持URI的"%HEXHEX"编码，如对 http://abc.com:80/~smith/ 和 [http://ABC.com/***\*%7E\****smith/](http://ABC.com/~smith/) 两种等价的URI能够正确处理；

\- 正确给出应答码（如200，304，100，404，500等）；

\- 支持Connection: Keep-Alive和Connection: Close两种连接模式。

服务器的基本要求包括：

A. 可配置Web服务器的监听地址、监听端口和虚拟路径。

B. 能够多线程处理并发的请求，或采取其他方法正确处理多个并发连接。

C. 对于无法成功定位文件的请求，根据错误原因，作相应错误提示。支持一定的异常情况处理能力。 

D. 服务可以启动和关闭。

E. 在服务器端的日志中记录每一个请求（如IP地址、端口号和HTTP请求命令行，及应答码等）。



### 日志系统

* 日志输出到文件，日志格式为

* 时间： ip 地址 端口号 HTTP请求行信息 

  ​	返回的应答信息与应答码。
  
  
  
  





### http处理

* http报文的分析

  GET, POST, HEAD

  ```
  
  ```
  
  
  
  通过分割'\r\n'得到每一行
  
  第一行为请求行 : 方法 URL 版本
  
  将第二行以下以及空行以上交给请求头处理\
  
  POST请求再分析请求正文
  
  
  
  char* buf -> char* response;
  
  支持GET, POST, HEAD
  
  请求报文
  
  包含：
  
  方法名	URL	HTTP版本

​	Host:

​	Connection:

​	Referer:

​	Accpet-Encoding: 

​	应答报文：

​	版本	状态码	协议

​	Cache-Control:(控制缓存以及超时时间)

​	Expires:

​	Content-Disposition:	;filename:(控制下载的方式以及文件名)

​	Content-Type:	(文件类型)

​	Connection:	(长短连接)

​	Content-Length:	()



* http报文的响应

  应答码包括{

  ​	200，304，100，404，500

  }

  响应的资源？长度？内容？

  