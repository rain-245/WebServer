CC = g++
CFLAGS = -std=c++11 -pthread -Wall
TARGET = web_server
# 删掉 threadpool.o，只保留用到的 .o 文件
OBJS = main.o http_conn.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp http_conn.h threadpool.h locker.h
	$(CC) $(CFLAGS) -c main.cpp

http_conn.o: http_conn.cpp http_conn.h
	$(CC) $(CFLAGS) -c http_conn.cpp

clean:
	rm -f $(TARGET) *.o