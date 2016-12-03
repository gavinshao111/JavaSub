# JavaSub
订阅以车机ID($1234)为主题的MQ，当收到MQ时解析，然后启动ffmpeg推流。
用到 fusesource 的MQTT库，Java启动进程，监听处理信号，互斥锁 Lock，Google的Gson库。
程序以 startup.sh 启动，并后台运行，日志输出到 stdout.log，并当接收到SIGINT或SIGTERM时退出。
