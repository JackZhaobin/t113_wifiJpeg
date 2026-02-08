# Makefile for JPEG Stream Transmission

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -static

# 目标文件
SERVER_TARGET = jpeg_stream_server
CLIENT_TARGET = jpeg_stream_client

# 源文件
SERVER_SRC = server.c
CLIENT_SRC = client.c

.PHONY: all clean server client install

all: server client

server: $(SERVER_TARGET)

client: $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(SERVER_TARGET) $(SERVER_SRC)
	@echo "服务端编译完成: $(SERVER_TARGET)"

$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SRC)
	@echo "客户端编译完成: $(CLIENT_TARGET)"

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo "清理完成"

install: server
	@echo "安装服务端到BusyBox系统..."
	@echo "请手动复制 $(SERVER_TARGET) 到目标系统"

help:
	@echo "可用目标:"
	@echo "  make all      - 编译服务端和客户端"
	@echo "  make server   - 仅编译服务端（BusyBox端）"
	@echo "  make client   - 仅编译客户端"
	@echo "  make clean    - 清理编译文件"
	@echo "  make install  - 显示安装说明"

