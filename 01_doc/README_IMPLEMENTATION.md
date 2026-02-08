# JPEG流传输实现说明

## 项目概述

实现BusyBox端实时传输JPEG图片流（15fps）到WiFi客户端。

- **服务端**：运行在BusyBox系统，每秒产生15帧JPEG并发送
- **客户端**：WiFi STA模式，连接后接收JPEG流

## 系统架构

```
[BusyBox服务端]  --WiFi-->  [客户端WiFi STA]
    (生成JPEG)              (接收并显示/保存)
```

## 文件说明

- `server.c` - 服务端代码（BusyBox端，C语言）
- `client.c` - 客户端代码（C语言版本）
- `client_python.py` - 客户端代码（Python版本，可选）
- `protocol.md` - 协议设计文档
- `Makefile` - 编译脚本

## 编译方法

### 1. 编译服务端（BusyBox端）

```bash
# 在开发机上交叉编译（如果BusyBox是ARM架构）
arm-linux-gnueabihf-gcc -static -Wall -O2 -o jpeg_stream_server server.c

# 或者在BusyBox系统上直接编译
gcc -static -Wall -O2 -o jpeg_stream_server server.c

# 使用Makefile
make server
```

### 2. 编译客户端

```bash
# C语言版本
gcc -Wall -O2 -o jpeg_stream_client client.c

# 或使用Makefile
make client

# Python版本（无需编译）
chmod +x client_python.py
```

## 使用方法

### 服务端（BusyBox端）

1. **准备JPEG测试图片**
   ```bash
   # 在BusyBox系统上准备一个测试JPEG文件
   # 例如：test.jpg
   ```

2. **启动服务端**
   ```bash
   # 基本用法
   ./jpeg_stream_server test.jpg 8080
   
   # 使用默认端口8080
   ./jpeg_stream_server test.jpg
   ```

3. **服务端输出**
   ```
   JPEG流服务器启动成功
   监听端口: 8080
   帧率: 15 fps
   JPEG文件: test.jpg
   等待客户端连接...
   ```

### 客户端（WiFi STA端）

#### C语言版本

```bash
# 基本用法（仅接收，不保存）
./jpeg_stream_client 192.168.1.100 8080

# 保存每一帧到文件
./jpeg_stream_client 192.168.1.100 8080 -s -d ./frames

# 接收指定数量的帧
./jpeg_stream_client 192.168.1.100 8080 -n 100

# 完整示例
./jpeg_stream_client 192.168.1.100 8080 -s -d ./output -n 150
```

#### Python版本

```bash
# 基本用法
python3 client_python.py 192.168.1.100 8080

# 保存每一帧
python3 client_python.py 192.168.1.100 8080 -s -d ./frames

# 接收指定数量
python3 client_python.py 192.168.1.100 8080 -n 100
```

## 网络配置

### BusyBox端（服务端）

1. **配置WiFi AP模式**（如果BusyBox作为AP）
   ```bash
   # 使用hostapd等工具配置WiFi AP
   # 设置IP地址，例如：192.168.1.1
   ifconfig wlan0 192.168.1.1 netmask 255.255.255.0
   ```

2. **或者连接到现有WiFi网络**
   ```bash
   # 使用wpa_supplicant连接到WiFi
   # 获取IP地址，例如：192.168.1.100
   ```

### 客户端（WiFi STA）

1. **连接到WiFi网络**
   ```bash
   # 连接到BusyBox的WiFi网络
   # 获取IP地址，例如：192.168.1.101
   ```

2. **测试连接**
   ```bash
   # 测试网络连通性
   ping 192.168.1.100
   
   # 测试端口
   telnet 192.168.1.100 8080
   ```

## 性能优化建议

### 1. JPEG图片大小
- 建议每帧JPEG大小控制在50-200KB
- 过大的图片会导致延迟增加
- 可以使用JPEG压缩质量参数调整

### 2. 网络带宽
- 15fps × 100KB/帧 ≈ 1.5MB/s ≈ 12Mbps
- 确保WiFi带宽足够（建议802.11n以上）

### 3. 服务端优化
- 使用非阻塞IO（可选）
- 调整TCP发送缓冲区大小
- 使用多线程支持多客户端（如需要）

### 4. 客户端优化
- 使用显示缓冲区，避免阻塞接收
- 实时显示时使用双缓冲
- 保存文件时使用异步IO

## 故障排查

### 1. 连接失败
```bash
# 检查网络连通性
ping <server_ip>

# 检查端口是否开放
telnet <server_ip> 8080

# 检查防火墙
iptables -L
```

### 2. 帧率不稳定
- 检查网络带宽是否足够
- 检查JPEG文件大小
- 检查系统负载

### 3. 丢帧问题
- 检查网络质量
- 增加接收缓冲区
- 检查客户端处理速度

## 扩展功能

### 1. 支持多客户端
修改`server.c`，使用多线程或select/poll支持多个客户端连接。

### 2. 动态JPEG生成
修改服务端，从摄像头实时捕获并编码为JPEG，而不是读取固定文件。

### 3. 压缩优化
- 使用硬件JPEG编码器（如果可用）
- 动态调整JPEG质量
- 使用MJPEG格式

### 4. 错误恢复
- 自动重连机制
- 帧序号检测和重传请求
- 网络质量监控

## 测试建议

1. **功能测试**
   ```bash
   # 服务端启动
   ./jpeg_stream_server test.jpg
   
   # 客户端连接并保存100帧
   ./jpeg_stream_client 192.168.1.100 8080 -s -n 100
   
   # 检查保存的帧
   ls -lh frames/
   ```

2. **性能测试**
   ```bash
   # 运行30秒，统计帧率
   timeout 30 ./jpeg_stream_client 192.168.1.100 8080
   ```

3. **压力测试**
   - 长时间运行测试稳定性
   - 测试网络中断恢复
   - 测试大图片传输

## 注意事项

1. **内存管理**：确保有足够内存存储JPEG数据
2. **文件系统**：保存帧时确保有足够存储空间
3. **网络稳定性**：WiFi连接可能不稳定，需要错误处理
4. **资源限制**：BusyBox系统资源有限，注意优化

