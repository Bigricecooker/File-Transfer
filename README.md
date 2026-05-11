# File Transfer

一个基于 C++ 的局域网文件传输工具，包含 Qt5 图形客户端和 Windows 控制台服务端。

## 架构

```
Client  (Qt5 GUI, WinSock2)  ──→  Server  (select, ThreadPool)
                                     └── 存储目录 (storage/)
                                     └── 日志目录 (logs/)
                                     └── 用户配置 (users.ini)
```

- **客户端**: Qt5 界面，支持登录、上传、下载、刷新文件列表
- **服务端**: 基于 `select` 的 I/O 多路复用 + 动态线程池处理任务，支持多客户端并发

## 功能

- 用户登录认证（用户名/密码）
- 文件上传（5MB 分片传输，CRC32 校验，服务端校验失败自动删除，同名文件拒绝覆盖）
- 文件下载（5MB 分片传输，CRC32 校验，客户端校验失败自动删除）
- 文件上传与下载进度条
- 文件列表刷新
- 服务端日志记录（文件 + 控制台）

## 环境要求

| 组件          | 要求                           |
| ------------- | ------------------------------ |
| 操作系统      | Windows 10+                    |
| Visual Studio | 2022 (v143) / 2019 (v142)      |
| Windows SDK   | 10.0                           |
| Qt            | 5.15.2 MSVC 2019 64bit         |
| 工具集        | `core`, `gui`, `widgets` |

## 构建

### 服务端 (Server)

1. 打开 `Server/Server.slnx`
2. 选择 `x64-Release` 配置
3. 生成解决方案

### 客户端 (Client)

1. 安装 Qt 5.15.2 MSVC 2019 64bit
2. 打开 `Client/Client.slnx`
3. 选择 `x64-Release` 配置
4. 生成解决方案

> 客户端需要配置 Qt MSBuild 集成，确保 `QtMsBuild` 路径正确。
> Debug 和 Release 配置均需添加 `/utf-8` 编译选项（`ClCompile → AdditionalOptions`），否则含中文的源文件会在 Release 模式下产生乱码。

## 配置

### 服务端

创建 `users.ini`（与服务端 exe 同目录）：

```ini
[users]
admin=123456
nihao=123
```

### 启动

```bash
# 服务端（默认端口 8080）
Server.exe

# 指定端口
Server.exe 9090
```

客户端启动后输入服务端 IP 和端口即可连接。

## 目录结构

```
File Transfer/
├── Client/              # Qt5 客户端
│   └── Client/
│       ├── Client.cpp/h      # 登录窗口
│       ├── MainWidget.cpp/h  # 主界面（上传/下载）
│       ├── const.cpp/h       # 网络协议定义
│       └── main.cpp
├── Server/              # 控制台服务端
│   └── Server/
│       ├── Server.cpp/h      # 核心服务逻辑
│       ├── ThreadPool.cpp/h  # 线程池
│       ├── MyCProFile.cpp/h  # INI 解析
│       ├── const.h           # 协议常量
│       └── mian.cpp          # 入口点
└── README.md
```

## 协议

| 消息 ID                | 方向 | 说明                              |
| ---------------------- | ---- | --------------------------------- |
| `MSG_LOGIN`          | C→S | 登录请求                          |
| `MSG_UPLOAD_REQ`     | C→S | 上传请求（含文件名、大小、CRC32）；若同名文件已存在，服务端返回 `MSG_RESP_ERR` |
| `MSG_UPLOAD_DATA`    | C→S | 上传数据分片                      |
| `MSG_DOWNLOAD_REQ`   | C→S | 下载请求                          |
| `MSG_DOWNLOAD_DATA`  | S→C | 下载数据分片                      |
| `MSG_GET_FILE_LIST`  | C→S | 请求文件列表                      |
| `MSG_FILE_LIST_RESP` | S→C | 文件列表响应                      |
| `MSG_RESP_OK/ERR`    | 双向 | 通用成功/失败响应                 |
| `MSG_RESP_OK`        | S→C | 上传完成且 CRC32 校验通过         |
| `MSG_RESP_ERR`       | S→C | 上传完成但 CRC32 校验失败，文件已被服务端删除 |

> CRC32 校验：上传时客户端流式计算 CRC32 后发送给服务端，服务端收完所有分片后校验整个文件，不匹配则删除；下载时服务端在响应中携带 CRC32，客户端逐分片增量校验，不匹配则删除已下载文件。
