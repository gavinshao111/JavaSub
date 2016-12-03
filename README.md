# JavaSub
订阅以车机ID($1234)为主题的MQ，当收到MQ时解析，然后启动ffmpeg推流。
用到 fusesource 的MQTT库，Java启动进程，监听处理信号，互斥锁 Lock，Google的Gson库。
程序以 startup.sh 启动，并后台运行，日志输出到 stdout.log，并当接收到SIGINT或SIGTERM时退出。

此项目目的是提供模拟推流。有一个MQTT客户端订阅MQ，有一个推流程序ffmpeg。MQTT客户端一开始在centOS下编译运行c++程序，而ffmpeg在Ubuntu下编译运行，彼此不能到对方的环境运行。
一开始忽略了这是两台机子上的程序，想用fifo通信。后来用socket通信，但是ffmpeg推流程序循环运行失败，只能让ffmpeg的进程结束再重启来达到控制推流开始停止的目的。
这样就得有个代理进程来管理ffmpeg进程，干脆让这个代理进程把MQTT客户端的活也干了算了。于是我又重写了MQTT客户端，这次选择了Java平台的fusesource作为MQTT客户端以减少开发工作。
现在就是在Ubuntu上运行java平台的MQTT客户端，来订阅MQ，然后收到MQ时启动/停止ffmpeg进程。