#ifndef CONST_H
#define CONST_H
#include <winsock2.h>
#include <cstdint>
#include <vector>
#include <QByteArray>
/// <summary>
/// 包头结构，包含消息ID和消息体长度，一共6字节
/// </summary>
struct MsgHeader
{
    uint16_t id;
    uint32_t len;
};

/// <summary>
/// 登录包
/// </summary>
struct LoginReq
{
    char user[32];
    char pwd[32];
};

/// <summary>
/// 上传请求包
/// </summary>
struct UploadReq
{
    char fileName[256];
    uint64_t fileSize;// 文件总大小（网络字节序）
    uint32_t crc32;// 文件CRC32（网络字节序）
};

/// <summary>
/// 下载请求包
/// </summary>
struct DownloadReq
{
    char fileName[256];
};

/// <summary>
/// 下载响应包
/// </summary>
struct DownloadRsp
{
    uint64_t fileSize;// 文件总大小（网络字节序）
};

/// <summary>
/// 上传数据包头
/// </summary>
struct UploadDataHeader
{
    uint32_t seq;// 分片序号（从0开始，网络字节序）
};

/// <summary>
/// 消息ID枚举
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


const int chunkSize = 5 * 1024 * 1024; // 5 MB

/// <summary>
/// 发送消息
/// </summary>
/// <param name="sock"></param>
/// <param name="id"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool sendPacket(SOCKET sock, uint16_t id, const void* data, uint32_t len);

/// <summary>
/// 接收消息
/// </summary>
/// <param name="sock"></param>
/// <param name="out_id"></param>
/// <param name="out_data"></param>
/// <param name="recvBuf"></param>
/// <returns></returns>
bool recvPacket(SOCKET sock, uint16_t& out_id, std::vector<char>& out_data, std::vector<char>& recvBuf);

/// <summary>
/// CRC32校验计算
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
uint32_t calculateCRC32(const QByteArray& data);



#endif // CONST_H