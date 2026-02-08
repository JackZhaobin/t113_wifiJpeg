/**
 * JPEG流传输服务端 (BusyBox端)
 * 功能：每秒产生15帧JPEG图片，通过TCP Socket发送给客户端
 * 编译：gcc -static -o jpeg_stream_server server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define PORT 8080
#define FPS 15
#define FRAME_INTERVAL_MS (1000 / FPS)  // 约66ms
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024 * 1024  // 1MB缓冲区

// 全局变量
static volatile int running = 1;
static int server_sock = -1;

// 信号处理
void signal_handler(int sig) {
    printf("\n收到信号，正在关闭服务器...\n");
    running = 0;
    if (server_sock >= 0) {
        close(server_sock);
    }
}

/**
 * 读取JPEG文件
 * 返回：文件大小，失败返回-1
 */
long read_jpeg_file(const char* filename, unsigned char** buffer) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    *buffer = malloc(file_size);
    if (!*buffer) {
        fclose(fp);
        return -1;
    }
    
    size_t read_size = fread(*buffer, 1, file_size, fp);
    fclose(fp);
    
    if (read_size != file_size) {
        free(*buffer);
        return -1;
    }
    
    return file_size;
}

/**
 * 发送一帧JPEG数据
 * 返回：0成功，-1失败
 */
int send_jpeg_frame(int client_sock, uint32_t frame_num, 
                    unsigned char* jpeg_data, long jpeg_size) {
    // 构造帧头（8字节）
    uint32_t frame_num_net = htonl(frame_num);
    uint32_t size_net = htonl((uint32_t)jpeg_size);
    
    // 发送帧头
    if (send(client_sock, &frame_num_net, 4, 0) != 4) {
        return -1;
    }
    if (send(client_sock, &size_net, 4, 0) != 4) {
        return -1;
    }
    
    // 分块发送JPEG数据（避免大块数据导致延迟）
    long sent = 0;
    while (sent < jpeg_size) {
        long chunk_size = (jpeg_size - sent > BUFFER_SIZE) ? 
                          BUFFER_SIZE : (jpeg_size - sent);
        
        ssize_t n = send(client_sock, jpeg_data + sent, chunk_size, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 缓冲区满，稍等再试
                usleep(1000);
                continue;
            }
            return -1;
        }
        sent += n;
    }
    
    return 0;
}

/**
 * 处理客户端连接
 */
void handle_client(int client_sock, const char* jpeg_file) {
    printf("客户端已连接，开始发送JPEG流...\n");
    
    // 读取JPEG文件
    unsigned char* jpeg_data = NULL;
    long jpeg_size = read_jpeg_file(jpeg_file, &jpeg_data);
    
    if (jpeg_size < 0) {
        printf("错误：无法读取JPEG文件 %s\n", jpeg_file);
        close(client_sock);
        return;
    }
    
    printf("JPEG文件大小: %ld 字节\n", jpeg_size);
    
    // 设置socket为非阻塞模式（可选，用于更好的性能）
    // int flags = fcntl(client_sock, F_GETFL, 0);
    // fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
    
    // 计算帧间隔（微秒）
    long frame_interval_us = FRAME_INTERVAL_MS * 1000;
    
    uint32_t frame_num = 0;
    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    while (running) {
        // 发送一帧
        if (send_jpeg_frame(client_sock, frame_num, jpeg_data, jpeg_size) < 0) {
            printf("发送失败，客户端可能已断开\n");
            break;
        }
        
        frame_num++;
        if (frame_num % 150 == 0) {  // 每10秒打印一次
            printf("已发送 %u 帧\n", frame_num);
        }
        
        // 精确控制帧率
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                         (current_time.tv_nsec - start_time.tv_nsec) / 1000;
        long expected_us = frame_num * frame_interval_us;
        long sleep_us = expected_us - elapsed_us;
        
        if (sleep_us > 0) {
            usleep(sleep_us);
        } else if (sleep_us < -frame_interval_us) {
            // 延迟过大，重置时间基准
            printf("警告：帧率延迟过大，重置时间基准\n");
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            frame_num = 0;
        }
    }
    
    free(jpeg_data);
    close(client_sock);
    printf("客户端连接已关闭\n");
}

/**
 * 主函数
 */
int main(int argc, char *argv[]) {
    int port = PORT;
    const char* jpeg_file = NULL;
    
    // 解析参数
    if (argc < 2) {
        printf("用法: %s <jpeg_file> [port]\n", argv[0]);
        printf("示例: %s test.jpg 8080\n", argv[0]);
        return 1;
    }
    
    jpeg_file = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 创建socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }
    
    // 设置socket选项（重用地址）
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }
    
    // 监听
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_sock);
        return 1;
    }
    
    printf("JPEG流服务器启动成功\n");
    printf("监听端口: %d\n", port);
    printf("帧率: %d fps\n", FPS);
    printf("JPEG文件: %s\n", jpeg_file);
    printf("等待客户端连接...\n");
    
    // 主循环：接受客户端连接
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (running) {
                perror("accept");
            }
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("新客户端连接: %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        // 处理客户端（当前版本只支持一个客户端）
        // 如需支持多客户端，需要使用多线程或select
        handle_client(client_sock, jpeg_file);
    }
    
    close(server_sock);
    printf("服务器已关闭\n");
    return 0;
}

