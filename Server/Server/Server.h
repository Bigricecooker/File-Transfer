#ifndef SERVER_H
#define SERVER_H

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <map>
#include "ThreadPool.h"

#include <vector>

#include <winsock2.h>
#include <windows.h>


/// <summary>
/// 监听端口，处理客户端连接
/// </summary>
class Server
{
public:
	Server();
	~Server();

	/// <summary>
	/// 初始化服务器，加载用户认证信息等准备工作
	/// </summary>
	void init();

	/// <summary>
	/// 设置监听端口
	/// </summary>
	/// <param name="port"></param>
	void SetListenPort(unsigned short port);

	/// <summary>
	/// 设置文件存储目录
	/// </summary>
	/// <param name="dir"></param>
	void SetStorageDir(std::wstring dir);

	/// <summary>
	/// 设置用户认证文件路径
	/// </summary>
	/// <param name="path"></param>
	void SetUsersIniPath(std::wstring path);

	/// <summary>
	/// 设置日志文件路径
	/// </summary>
	/// <param name="path"></param>
	void SetLogPath(std::wstring path);

	/// <summary>
	/// 开始服务
	/// </summary>
	void Start();

    /// <summary>
	/// 关闭服务
    /// </summary>
    void Stop();


private:
	struct ClientContext
	{
		SOCKET sock = INVALID_SOCKET;
		sockaddr_in addr{};
		bool authed = false;
		std::string user;

		// 接收/发送缓冲区
		std::vector<uint8_t> recvBuf;
		std::vector<uint8_t> sendBuf;

		// 状态和控制标志
		bool handling = false; // 是否已有线程在处理网络读写
		std::mutex mtx; // 用于保护当前ClientContext实例免受并发写入

		// upload state
		bool uploading = false;// 是否正在上传文件
		std::wstring uploadFileName;// 正在上传的文件名
		uint64_t uploadTotalSize = 0;// 文件总大小
		uint64_t uploadReceived = 0;// 已接收的上传数据大小
		uint32_t expectedCrc = 0;// 客户端发送的CRC32
		HANDLE uploadFileHandle = INVALID_HANDLE_VALUE;// 文件句柄，用于写入上传数据

		// download state
		bool downloading = false;
		std::wstring downloadFileName;
		uint64_t downloadTotalSize = 0;
		uint64_t downloadSent = 0;
		uint32_t downloadSeq = 0;
		HANDLE downloadFileHandle = INVALID_HANDLE_VALUE;
	};

	/// <summary>
	/// 任务对象，包含客户端套接字、消息ID和消息体数据，用于在线程池中处理客户端请求。
	/// </summary>
	struct Task
	{
		SOCKET sock = INVALID_SOCKET;
		uint16_t msgId = 0;
		std::vector<uint8_t> payload;
	};

	/// <summary>
	/// 初始化Winsock库
	/// </summary>
	/// <returns></returns>
	bool InitWinsock();

	/// <summary>
	/// 清理Winsock库
	/// </summary>
	void CleanupWinsock();

	/// <summary>
	/// 创建并设置为监听状态的套接字，用于接收传入连接。
	/// </summary>
	/// <returns></returns>
	bool CreateListenSocket();

	/// <summary>
	/// 关闭监听套接字
	/// </summary>
	void CloseListenSocket() ;

	/// <summary>
	/// 事件循环，使用select监视套接字事件
	/// </summary>
	void MainLoop();
	/// <summary>
	/// 接收新连接，并将其添加到客户端列表中
	/// </summary>
	void AcceptNewClient();
	void HandleReadable(SOCKET s);
	void HandleWritable(SOCKET s);

	/// <summary>
	/// 关闭客户端连接，释放相关资源，并从客户端列表中移除
	/// </summary>
	/// <param name="s"></param>
	/// <param name="reason"></param>
	void DisconnectClient(SOCKET s, const char* reason);

	// protocol helpers

	/// <summary>
	/// 解析客户端发送的数据包，提取消息ID和消息体，并将其封装为任务对象以供逻辑处理函数使用。
	/// </summary>
	/// <param name="ctx"></param>
	/// <param name="outTasks"></param>
	/// <returns></returns>
	bool TryParsePackets(ClientContext& ctx, std::vector<Task>& outTasks);

	bool SendPending(ClientContext& ctx);
	
	bool ReadIntoBuffer(ClientContext& ctx);
	
	/// <summary>
	/// 任务处理函数，实际项目中应该根据msgId分发到不同的处理函数
	/// </summary>
	/// <param name="task"></param>
	void ProcessTask(Task task);
	void HandleLogin(Task& task, ClientContext& ctx);
	void HandleUploadReq(Task& task, ClientContext& ctx);// 上传请求处理函数
	void HandleUploadData(Task& task, ClientContext& ctx);// 上传数据处理函数
	void HandleRefreshReq(Task& task, ClientContext& ctx);
	void HandleDownloadReq(Task& task, ClientContext& ctx);// 下载请求处理函数
	void QueueNextDownloadChunk(ClientContext& ctx);// 排队发送下一个下载分片

	void WriteLog(const char* fmt, ...);// 写入日期日志文件


	unsigned short m_port;// 监听端口
	std::atomic_bool m_running;// 是否正在运行
	SOCKET m_listenSock;// 监听socket

	fd_set m_readSet;// select用的读事件集合
	fd_set m_writeSet;// select用的写事件集合
	std::mutex m_clientsMtx;
	std::unordered_map<SOCKET, std::shared_ptr<ClientContext>> m_clients;// 活跃的客户端连接

	ThreadPool m_pool;

	std::wstring m_storageDir = L"storage";
	std::wstring m_usersIniPath = L"users.ini";
	std::wstring m_logPath = L"server.log";
	std::map<std::string, std::string> m_users;// 用户认证信息，user->pass
	mutable std::mutex m_logMtx;
	mutable std::mutex m_usersMtx;

	WSADATA m_wsa{};
};
#endif 



