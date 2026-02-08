#!/bin/bash
# 测试网络连接脚本

if [ $# -lt 1 ]; then
    echo "用法: $0 <server_ip> [port]"
    echo "示例: $0 192.168.1.100 8080"
    exit 1
fi

SERVER_IP=$1
PORT=${2:-8080}

echo "测试连接到 $SERVER_IP:$PORT..."

# 测试ping
echo -n "1. 测试ping... "
if ping -c 1 -W 2 $SERVER_IP > /dev/null 2>&1; then
    echo "✓ 成功"
else
    echo "✗ 失败"
    echo "   请检查WiFi连接和IP地址"
    exit 1
fi

# 测试端口
echo -n "2. 测试端口 $PORT... "
if timeout 2 bash -c "echo > /dev/tcp/$SERVER_IP/$PORT" 2>/dev/null; then
    echo "✓ 端口开放"
else
    echo "✗ 端口未开放或服务未启动"
    echo "   请确保服务端已启动: ./jpeg_stream_server test.jpg $PORT"
    exit 1
fi

# 测试telnet连接（如果可用）
if command -v telnet > /dev/null 2>&1; then
    echo -n "3. 测试TCP连接... "
    if timeout 2 telnet $SERVER_IP $PORT </dev/null 2>/dev/null | grep -q "Connected"; then
        echo "✓ 连接成功"
    else
        echo "✗ 连接失败"
    fi
fi

echo ""
echo "网络连接测试完成！"
echo "可以运行客户端: ./jpeg_stream_client $SERVER_IP $PORT"

