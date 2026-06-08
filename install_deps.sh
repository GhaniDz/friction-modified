#!/bin/bash
set -e

echo "=== 1. 添加 FFmpeg 7.x PPA ==="
sudo add-apt-repository -y ppa:ubuntuhandbook1/ffmpeg7
sudo apt-get update

echo "=== 2. 安装 FFmpeg 7.x 库 ==="
sudo apt-get install -y \
  libavformat62 \
  libavcodec62 \
  libavutil60 \
  libswscale9 \
  libswresample6

echo "=== 3. 安装编译工具 ==="
sudo apt-get install -y gcc

echo "=== 依赖安装完成 ==="
