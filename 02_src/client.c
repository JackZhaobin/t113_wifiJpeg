/**
 * JPEG流传输客户端
 * 功能：连接到BusyBox服务端，接收JPEG图片流并保存
 * 编译：gcc -o jpeg_stream_client client.c
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
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024 * 1024  // 1MB缓冲区
#define MAX_JPEG_SIZE 10 * 1024 * 1024  // 最大JPEG大小10MB

/**
 * 接收一帧JPEG数据
 * 返回：0成功，-1失败
 */
int receive_jpeg_frame(int sock, uint32_t* frame_num, 
                       unsigned char** jpeg_data, long* jpeg_size) {
    // 接收帧头（8字节）
    uint32_t frame_num_net, size_net;
    
    // 接收帧序号（4字节）
    ssize_t n = recv(sock, &frame_num_net, 4, MSG_WAITALL);
    if (n != 4) {
        if (n == 0) {
            printf("连接已关闭\n");
        } else {
            perror("recv frame_num");
        }
        return -1;
    }
    
    // 接收数据长度（4字节）
    n = recv(sock, &size_net, 4, MSG_WAITALL);
    if (n != 4) {
        perror("recv size");
        return -1;
    }
    
    *frame_num = ntohl(frame_num_net);
    *jpeg_size = ntohl(size_net);
    
    // 检查大小是否合理
    if (*jpeg_size == 0 || *jpeg_size > MAX_JPEG_SIZE) {
        printf("错误：无效的JPEG大小 %ld\n", *jpeg_size);
        return -1;
    }
    
    // 分配内存
    *jpeg_data = malloc(*jpeg_size);
    if (!*jpeg_data) {
        printf("错误：内存分配失败\n");
        return -1;
    }
    
    // 接收JPEG数据
    long received = 0;
    while (received < *jpeg_size) {
        n = recv(sock, (*jpeg_data) + received, *jpeg_size - received, 0);
        if (n <= 0) {
            if (n == 0) {
                printf("连接已关闭\n");
            } else {
                perror("recv jpeg_data");
            }
            free(*jpeg_data);
            return -1;
        }
        received += n;
    }
    
    return 0;
}

/**
 * 保存JPEG文件
 */
int save_jpeg_file(const char* filename, unsigned char* data, long size) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    if (written != size) {
        printf("错误：写入文件不完整\n");
        return -1;
    }
    
    return 0;
}

/**
 * 主函数
 */
int main(int argc, char *argv[]) {
    const char* server_ip = NULL;
    int port = 8080;
    const char* output_dir = "./frames";
    int save_frames = 0;  // 是否保存每一帧
    int max_frames = 0;   // 最大接收帧数（0表示无限制）
    
    // 解析参数
    if (argc < 2) {
        printf("用法: %s <server_ip> [port] [options]\n", argv[0]);
        printf("选项:\n");
        printf("  -s, --save       保存每一帧到文件\n");
        printf("  -d, --dir DIR    保存目录（默认: ./frames）\n");
        printf("  -n, --num NUM    最大接收帧数（默认: 无限制）\n");
        printf("\n示例:\n");
        printf("  %s 192.168.1.100 8080\n", argv[0]);
        printf("  %s 192.168.1.100 8080 -s -d ./output -n 100\n", argv[0]);
        return 1;
    }
    
    server_ip = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    // 解析选项
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--save") == 0) {
            save_frames = 1;
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) && i + 1 < argc) {
            output_dir = argv[++i];
        } else if ((strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--num") == 0) && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        }
    }
    
    // 创建输出目录
    if (save_frames) {
        struct stat st = {0};
        if (stat(output_dir, &st) == -1) {
            if (mkdir(output_dir, 0755) < 0) {
                perror("mkdir");
                return 1;
            }
        }
        printf("帧将保存到: %s\n", output_dir);
    }
    
    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    // 连接服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("错误：无效的IP地址 %s\n", server_ip);
        close(sock);
        return 1;
    }
    
    printf("正在连接到 %s:%d...\n", server_ip, port);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    printf("连接成功！开始接收JPEG流...\n");
    
    // 接收循环
    uint32_t frame_count = 0;
    uint32_t last_frame_num = 0;
    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    while (1) {
        uint32_t frame_num;
        unsigned char* jpeg_data = NULL;
        long jpeg_size = 0;
        
        // 接收一帧
        if (receive_jpeg_frame(sock, &frame_num, &jpeg_data, &jpeg_size) < 0) {
            break;
        }
        
        frame_count++;
        
        // 检测丢帧
        if (frame_count > 1 && frame_num != last_frame_num + 1) {
            printf("警告：检测到丢帧 (期望: %u, 收到: %u)\n", 
                   last_frame_num + 1, frame_num);
        }
        last_frame_num = frame_num;
        
        // 打印统计信息
        if (frame_count % 150 == 0) {  // 每10秒打印一次
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            double elapsed = (current_time.tv_sec - start_time.tv_sec) +
                           (current_time.tv_nsec - start_time.tv_nsec) / 1e9;
            double fps = frame_count / elapsed;
            printf("已接收 %u 帧, 平均帧率: %.2f fps, 当前帧大小: %ld 字节\n",
                   frame_count, fps, jpeg_size);
        }
        
        // 保存帧（如果需要）
        if (save_frames) {
            char filename[256];
            snprintf(filename, sizeof(filename), "%s/frame_%06u.jpg", 
                    output_dir, frame_num);
            if (save_jpeg_file(filename, jpeg_data, jpeg_size) == 0) {
                // 只保存最新的一帧，删除旧的（可选）
                // 或者保存所有帧
            }
        }
        
        free(jpeg_data);
        
        // 检查是否达到最大帧数
        if (max_frames > 0 && frame_count >= max_frames) {
            printf("已达到最大帧数 %d，退出\n", max_frames);
            break;
        }
    }
    
    close(sock);
    
    // 打印最终统计
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double elapsed = (current_time.tv_sec - start_time.tv_sec) +
                    (current_time.tv_nsec - start_time.tv_nsec) / 1e9;
    double fps = (elapsed > 0) ? frame_count / elapsed : 0;
    
    printf("\n接收完成\n");
    printf("总帧数: %u\n", frame_count);
    printf("总时间: %.2f 秒\n", elapsed);
    printf("平均帧率: %.2f fps\n", fps);
    
    return 0;
}

