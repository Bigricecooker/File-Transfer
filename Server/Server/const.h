#ifndef CONST_H
#define CONST_H

/// <summary>
/// 逻辑处理ID
/// </summary>
enum MsgID
{
    MSG_LOGIN,  // 登录请求
    MSG_UPLOAD_REQ,  // 上传请求
    MSG_UPLOAD_DATA,  // 文件数据
    MSG_DOWNLOAD_REQ,  // 下载请求
    MSG_DOWNLOAD_DATA,  // 下载数据
    MSG_GET_FILE_LIST,   // 请求文件列表
    MSG_FILE_LIST_RESP,    // 文件列表响应
    MSG_RESP_OK,  // 成功
    MSG_RESP_ERR,  // 失败
};

// 上传请求包（网络传输格式）
struct UploadReq
{
    char fileName[256];
    uint64_t fileSize;        // 文件总大小（网络字节序）
    uint32_t crc32;           // 文件CRC32（网络字节序）
};

// 上传数据包头
struct UploadDataHeader
{
    uint32_t seq; // 分片序号（从0开始，网络字节序）
};

// 下载请求包
struct DownloadReq
{
    char fileName[256];
};

// 下载响应包（网络传输格式）
struct DownloadRsp
{
    uint64_t fileSize;        // 文件总大小（网络字节序）
};

// 下载数据包头
struct DownloadDataHeader
{
    uint32_t seq; // 分片序号（从0开始，网络字节序）
};

struct MsgHeader
{
    uint16_t id;
    uint32_t len;
};

// 登录包
struct LoginReq
{
    char user[32];
    char pwd[32];
};

constexpr size_t FTA_MAX_PAYLOAD = 16 * 1024 * 1024; // 16MB

const int CHUNKSIZE = 5 * 1024 * 1024; // 5MB 每个分包


#endif // CONST_H