#include "Server.h"
#include "const.h"
#include "MyCProFile.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")



struct FileInfo
{
	char fileName[256];
};


namespace
{
	constexpr int kSelectTimeoutMs = 200;

	static std::string GetDateTimeStr()
	{
		auto now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(now);
		tm local{};
		localtime_s(&local, &tt);
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
			1900 + local.tm_year, 1 + local.tm_mon, local.tm_mday,
			local.tm_hour, local.tm_min, local.tm_sec);
		return buf;
	}

	static std::string GetDateStr()
	{
		auto now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(now);
		tm local{};
		localtime_s(&local, &tt);
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
			1900 + local.tm_year, 1 + local.tm_mon, local.tm_mday);
		return buf;
	}

	static std::string SockAddrStr(const sockaddr_in& addr)
	{
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%s:%d", ip, ntohs(addr.sin_port));
		return buf;
	}

	static uint64_t FileSize64(HANDLE h)
	{
		LARGE_INTEGER li{};
		if (!GetFileSizeEx(h, &li))
			return 0;
		return static_cast<uint64_t>(li.QuadPart);
	}

	static bool EnsureDirExists(const std::wstring& dir)
	{
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(dir), ec);
		return !ec;
	}

	static uint32_t CalculateFileCRC32(const std::wstring& filePath)
	{
		HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
			return 0;

		static uint32_t table[256];
		static std::once_flag tableInit;
		std::call_once(tableInit, []() {
			for (int i = 0; i < 256; ++i) {
				uint32_t c = i;
				for (int j = 0; j < 8; ++j)
					c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
				table[i] = c;
			}
		});

		uint32_t crc = 0xFFFFFFFF;
		uint8_t buf[65536];
		DWORD bytesRead = 0;
		while (ReadFile(hFile, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
			for (DWORD i = 0; i < bytesRead; ++i)
				crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
		}
		CloseHandle(hFile);
		return crc ^ 0xFFFFFFFF;
	}
}

Server::Server()
	: m_port(8080), m_running(false), m_listenSock(INVALID_SOCKET)
{
	init();
}

Server::~Server()
{
	Stop();
}

void Server::init()
{
	// 加载用户认证信息
	MyCProFile usersIni;
	if (!usersIni.LoadFromFile("users.ini"))
		std::cerr << "Warning: users.ini not found or invalid, all logins will be rejected" << std::endl;
	m_users = usersIni.GetKeyValueMapBySection("users");
}

void Server::SetListenPort(unsigned short port)
{
	m_port = port;
}

void Server::SetStorageDir(std::wstring dir)
{
	if (!dir.empty())
		m_storageDir = std::move(dir);
}

void Server::SetUsersIniPath(std::wstring path)
{
	if (!path.empty())
		m_usersIniPath = std::move(path);
}

void Server::SetLogPath(std::wstring path)
{
	if (!path.empty())
		m_logPath = std::move(path);
}

bool Server::InitWinsock()
{
	const int rc = WSAStartup(MAKEWORD(2, 2), &m_wsa);
	return rc == 0;
}

void Server::CleanupWinsock()
{
	WSACleanup();
}

bool Server::CreateListenSocket()
{
	m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listenSock == INVALID_SOCKET)
		return false;

	BOOL reuse = TRUE;
	setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

	sockaddr_in sin{};
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(m_port);

	if (bind(m_listenSock, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
		return false;
	if (listen(m_listenSock, SOMAXCONN) == SOCKET_ERROR)
		return false;

	u_long nb = 1;
	ioctlsocket(m_listenSock, FIONBIO, &nb);
	return true;
}

void Server::CloseListenSocket()
{
	if (m_listenSock != INVALID_SOCKET)
	{
		closesocket(m_listenSock);
		m_listenSock = INVALID_SOCKET;
	}
}

void Server::Start()
{
	if (m_running.exchange(true))
		return;

	if (!InitWinsock())
	{
		m_running = false;
		std::cerr << "WSAStartup failed" << std::endl;
		return;
	}

	if (!EnsureDirExists(m_storageDir))
		std::cerr << "Warning: create storage dir failed" << std::endl;


	if (!CreateListenSocket())
	{
		std::cerr << "Create/bind/listen failed" << std::endl;
		CloseListenSocket();
		CleanupWinsock();
		m_running = false;
		return;
	}

	m_pool.Start(2, std::max<size_t>(4, std::thread::hardware_concurrency()));

	std::cout << "Server listening on port " << m_port << std::endl;
	WriteLog("[SERVER] Started on port %u", m_port);
	MainLoop();
}

void Server::Stop() 
{
	bool wasRunning = m_running.exchange(false);
	if (!wasRunning)
		return;

	m_pool.Stop();
	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		for (auto& kv : m_clients)
		{
			std::lock_guard<std::mutex> ctxLk(kv.second->mtx);
			if (kv.second->uploadFileHandle != INVALID_HANDLE_VALUE)
				CloseHandle(kv.second->uploadFileHandle);
			if (kv.second->downloadFileHandle != INVALID_HANDLE_VALUE)
				CloseHandle(kv.second->downloadFileHandle);
			if (kv.first != INVALID_SOCKET)
				closesocket(kv.first);
		}
		m_clients.clear();
	}

	CloseListenSocket();
	CleanupWinsock();

	WriteLog("[SERVER] Stopped");
}

void Server::MainLoop()
{
	while (m_running)
	{
		FD_ZERO(&m_readSet);
		FD_ZERO(&m_writeSet);
		FD_SET(m_listenSock, &m_readSet);

		std::unique_lock<std::mutex> lk(m_clientsMtx);
		for (auto& kv : m_clients)
		{
			FD_SET(kv.first, &m_readSet);
			std::lock_guard<std::mutex> ctxLk(kv.second->mtx);
			if (!kv.second->sendBuf.empty())
				FD_SET(kv.first, &m_writeSet);

		}
		lk.unlock();

		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = kSelectTimeoutMs * 1000;
		const int rc = select(0, &m_readSet, &m_writeSet, nullptr, &tv);
		if (rc == SOCKET_ERROR)
			continue;
		if (rc == 0)
			continue;

		// 处理新连接
		if (FD_ISSET(m_listenSock, &m_readSet))
			AcceptNewClient();

		std::vector<SOCKET> readable;
		std::vector<SOCKET> writable;
		
		lk.lock();
		readable.reserve(m_clients.size());
		writable.reserve(m_clients.size());
		for (auto& kv : m_clients)
		{
			if (FD_ISSET(kv.first, &m_readSet))
				readable.push_back(kv.first);
			if (FD_ISSET(kv.first, &m_writeSet))
				writable.push_back(kv.first);
		}
		lk.unlock();
		
		for (SOCKET s : readable)
		{
			m_pool.Enqueue([this, s]() { HandleReadable(s); });
		}
		for (SOCKET s : writable)
		{
			m_pool.Enqueue([this, s]() { HandleWritable(s); });
		}
	}
}

void Server::AcceptNewClient()
{
	for (;;)
	{
		sockaddr_in addr;
		int len = sizeof(addr);
		SOCKET s = accept(m_listenSock, (sockaddr*)&addr, &len);
		if (s == INVALID_SOCKET)
		{
			int e = WSAGetLastError();
			if (e == WSAEWOULDBLOCK)
				return;
			return;
		}

		// 设置为非阻塞
		u_long nb = 1;
		ioctlsocket(s, FIONBIO, &nb);

		// 添加到客户端列表
		auto ctx = std::make_shared<ClientContext>();
		ctx->sock = s;
		ctx->addr = addr;
		ctx->recvBuf.reserve(64 * 1024);
		{
			std::lock_guard<std::mutex> lk(m_clientsMtx);
			m_clients.emplace(s, ctx);
		}

		WriteLog("[CONNECT] %s sock=%llu", SockAddrStr(addr).c_str(), (unsigned long long)s);
	}
}

void Server::HandleReadable(SOCKET s)
{
	std::shared_ptr<ClientContext> pctx;
	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		auto it = m_clients.find(s);
		if (it == m_clients.end())
			return;

		if (it->second->handling)
			return; // 已经在一个线程处理该客户端的数据，放弃重复投递处理
		it->second->handling = true; // 标记处理中

		pctx = it->second;
	}

	std::unique_lock<std::mutex> ctxLk(pctx->mtx);
	if (!ReadIntoBuffer(*pctx))
	{
		ctxLk.unlock();
		DisconnectClient(s, "recv failed");
		return;
	}

	std::vector<Task> tasks;
	if (!TryParsePackets(*pctx, tasks))
	{
		ctxLk.unlock();
		DisconnectClient(s, "bad packet");
		return;
	}
	ctxLk.unlock();

	for (auto& t : tasks)
	{
		ProcessTask(std::move(t));
	}

	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		auto it = m_clients.find(s);
		if (it != m_clients.end())
			it->second->handling = false; // 处理结束
	}
}

void Server::HandleWritable(SOCKET s)
{
	std::shared_ptr<ClientContext> pctx;
	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		auto it = m_clients.find(s);
		if (it == m_clients.end())
			return;
		pctx = it->second;
	}

	std::unique_lock<std::mutex> ctxLk(pctx->mtx);
	if (!SendPending(*pctx))
	{
		ctxLk.unlock();
		DisconnectClient(s, "send failed");
		return;
	}
	QueueNextDownloadChunk(*pctx);
}

void Server::DisconnectClient(SOCKET s, const char* reason)
{
	std::string user;
	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		auto it = m_clients.find(s);
		if (it == m_clients.end())
			return;

		std::lock_guard<std::mutex> ctxLk(it->second->mtx);
		user = it->second->user;

		if (it->second->uploadFileHandle != INVALID_HANDLE_VALUE)
			CloseHandle(it->second->uploadFileHandle);
		if (it->second->downloadFileHandle != INVALID_HANDLE_VALUE)
			CloseHandle(it->second->downloadFileHandle);

		closesocket(s);
		m_clients.erase(it);
	}

	WriteLog("[DISCONNECT] sock=%llu user=%s reason=%s", (unsigned long long)s, user.empty() ? "unknown" : user.c_str(), reason ? reason : "unknown");
	printf("[%s] [DISCONNECT] sock=%llu user=%s reason=%s\n", GetDateTimeStr().c_str(), (unsigned long long)s, user.empty() ? "unknown" : user.c_str(), reason ? reason : "unknown");
}

bool Server::ReadIntoBuffer(ClientContext& ctx)
{
	uint8_t tmp[8192];
	int totalRecv = 0;
	for (;;)
	{
		int n = recv(ctx.sock, (char*)tmp, (int)sizeof(tmp), 0);
		if (n > 0)
		{
			totalRecv += n;
			ctx.recvBuf.insert(ctx.recvBuf.end(), tmp, tmp + n);
			if (ctx.recvBuf.size() > (FTA_MAX_PAYLOAD + sizeof(MsgHeader)) * 4)
				return false;
			continue;
		}
		if (n == 0) {
			if (totalRecv > 0)
				WriteLog("[RECV] sock=%llu bytes=%d", (unsigned long long)ctx.sock, totalRecv);
			return false;
		}
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK) {
			if (totalRecv > 0)
				WriteLog("[RECV] sock=%llu bytes=%d", (unsigned long long)ctx.sock, totalRecv);
			return true;
		}
		return false;
	}
}

bool Server::SendPending(ClientContext& ctx)
{
	int totalSent = 0;
	while (!ctx.sendBuf.empty())
	{
		int n = send(ctx.sock, (const char*)ctx.sendBuf.data(), (int)ctx.sendBuf.size(), 0);
		if (n > 0)
		{
			totalSent += n;
			ctx.sendBuf.erase(ctx.sendBuf.begin(), ctx.sendBuf.begin() + n);
			continue;
		}
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK) {
			if (totalSent > 0)
				WriteLog("[SEND] sock=%llu bytes=%d", (unsigned long long)ctx.sock, totalSent);
			return true;
		}
		return false;
	}
	if (totalSent > 0)
		WriteLog("[SEND] sock=%llu bytes=%d", (unsigned long long)ctx.sock, totalSent);
	return true;
}


bool Server::TryParsePackets(ClientContext& ctx, std::vector<Task>& outTasks)
{
	for (;;)
	{
		// 1. 缓冲区不够解析【包头 MsgHeader】(6字节) → 退出等待更多数据
		if (ctx.recvBuf.size() < sizeof(MsgHeader))
			return true;

		// 2. 从缓冲区拷贝出包头（你的客户端包头格式）
		MsgHeader hdr;
		memcpy(&hdr, ctx.recvBuf.data(), sizeof(MsgHeader));

		// 3. 网络字节序 → 主机字节序（必须转！）
		uint16_t msgId = ntohs(hdr.id);
		uint32_t len = ntohl(hdr.len);

		// 5. 不够一个完整包 → 退出等待更多数据
		if (ctx.recvBuf.size() < sizeof(MsgHeader) + len)
			return true;

		Task task;
		task.sock = ctx.sock;
		task.msgId = msgId;

		// 拷贝包体数据
		if (len > 0)
		{
			const uint8_t* payloadData = ctx.recvBuf.data() + sizeof(MsgHeader);
			task.payload.assign(payloadData, payloadData + len);
		}

		// 加入任务列表
		outTasks.push_back(std::move(task));

		WriteLog("[PARSE] sock=%llu msgId=%u payloadLen=%u", (unsigned long long)ctx.sock, msgId, len);

		// 6. 从接收缓冲区删除已解析的数据（关键！）
		uint32_t totalPacketSize = sizeof(MsgHeader) + len;
		ctx.recvBuf.erase(ctx.recvBuf.begin(), ctx.recvBuf.begin() + totalPacketSize);
	}
}


void Server::ProcessTask(Task task)
{
	std::shared_ptr<ClientContext> pctx;
	{
		std::lock_guard<std::mutex> lk(m_clientsMtx);
		auto it = m_clients.find(task.sock);
		if (it == m_clients.end())
			return;
		pctx = it->second;
	}

	switch (task.msgId)
	{
	case MSG_LOGIN:
		HandleLogin(task, *pctx);
		break;
	case MSG_GET_FILE_LIST:
		HandleRefreshReq(task, *pctx);
		break;
	case MSG_UPLOAD_REQ:
		HandleUploadReq(task, *pctx);
		break;
	case MSG_UPLOAD_DATA:
		HandleUploadData(task, *pctx);
		break;
	case MSG_DOWNLOAD_REQ:
		HandleDownloadReq(task, *pctx);
		break;
	default:
		break;
	}
}

void Server::HandleLogin(Task& task, ClientContext& ctx)
{
	if (task.payload.size() < sizeof(LoginReq))
		return;

	// 解析登录请求包
	LoginReq req;
	memcpy(&req, task.payload.data(), sizeof(LoginReq));
	std::string user(req.user);
	std::string pass(req.pwd);

	// 验证用户名和密码
	bool ok = (m_users.find(user) != m_users.end() && m_users[user] == pass);

	WriteLog("[LOGIN] sock=%llu user=%s result=%s", (unsigned long long)ctx.sock, user.c_str(), ok ? "OK" : "FAIL");
	printf("[%s] [LOGIN] sock=%llu user=%s result=%s\n", GetDateTimeStr().c_str(), (unsigned long long)ctx.sock, user.c_str(), ok ? "OK" : "FAIL");

	// 更新客户端认证状态
	std::lock_guard<std::mutex> ctxLk(ctx.mtx);
	ctx.authed = ok;
	ctx.user = ok ? user : std::string{};

	// 回复登录结果
	MsgHeader respHeader;
	respHeader.id = htons(ok ? MSG_RESP_OK : MSG_RESP_ERR);
	respHeader.len = htonl(0);

	const uint8_t* p = reinterpret_cast<const uint8_t*>(&respHeader);
	ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(respHeader));
}

void Server::HandleUploadReq(Task& task, ClientContext& ctx)
{
	std::lock_guard<std::mutex> ctxLk(ctx.mtx);

	// 检查认证
	if (!ctx.authed)
	{
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	if (task.payload.size() < sizeof(UploadReq))
		return;

	UploadReq req;
	memcpy(&req, task.payload.data(), sizeof(UploadReq));

	std::string rawName(req.fileName, strnlen(req.fileName, sizeof(req.fileName)));
	if (rawName.empty())
	{
		// 发送错误响应
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 安全检查：防止路径穿越
	std::wstring wFileName;
	int len = MultiByteToWideChar(CP_UTF8, 0, rawName.c_str(), -1, nullptr, 0);
	if (len > 0)
	{
		wFileName.resize(len - 1);  // 去掉结尾 null
		MultiByteToWideChar(CP_UTF8, 0, rawName.c_str(), -1, &wFileName[0], len);
	}
	if (wFileName.empty() ||
		wFileName.find(L"/") != std::wstring::npos ||
		wFileName.find(L"\\") != std::wstring::npos ||
		wFileName == L"." || wFileName == L"..")
	{
		// 非法文件名
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	std::wstring fullPath = m_storageDir + L"\\" + wFileName;

	// 如果正在上传其他文件，先关闭旧句柄（防御）
	if (ctx.uploadFileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ctx.uploadFileHandle);
		ctx.uploadFileHandle = INVALID_HANDLE_VALUE;
	}

	const uint64_t totalSize = ntohll(req.fileSize);
	

	// 创建文件（覆盖写）
	HANDLE hFile = CreateFileW(
		fullPath.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (hFile == INVALID_HANDLE_VALUE) {
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 预分配文件大小（避免碎片，支持乱序写入）
	LARGE_INTEGER fileSize;
	fileSize.QuadPart = static_cast<LONGLONG>(totalSize);
	if (!SetFilePointerEx(hFile, fileSize, NULL, FILE_BEGIN) ||
		!SetEndOfFile(hFile)) {
		// 预分配失败，清理并返回错误
		CloseHandle(hFile);
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 设置上传状态
	ctx.uploading = true;
	ctx.uploadFileName = wFileName;
	ctx.uploadTotalSize = totalSize;
	ctx.uploadReceived = 0;
	ctx.expectedCrc = ntohl(req.crc32);
	ctx.uploadFileHandle = hFile;

	WriteLog("[UPLOAD_REQ] sock=%llu file=%s size=%llu crc=%08X",
		(unsigned long long)ctx.sock, rawName.c_str(),
		(unsigned long long)totalSize, ntohl(req.crc32));
	printf("[%s] [RECV UPLOAD_REQ] file=%s size=%llu crc=%08X\n",
		GetDateTimeStr().c_str(), rawName.c_str(),
		(unsigned long long)totalSize, ntohl(req.crc32));

	// 回复成功
	MsgHeader hdr;
	hdr.id = htons(MSG_RESP_OK);
	hdr.len = htonl(0);
	const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
	ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
}

void Server::HandleUploadData(Task& task, ClientContext& ctx)
{
	std::lock_guard<std::mutex> lk(ctx.mtx);

	if (!ctx.uploading || ctx.uploadFileHandle == INVALID_HANDLE_VALUE)
	{
		//PushResponse(ctx, MSG_RESP_ERR); // 用你的方式发错误
		return;
	}

	const char* payload = reinterpret_cast<const char*>(task.payload.data());
	uint32_t payloadLen = task.payload.size();

	// 2. 检查长度
	if (payloadLen < sizeof(UploadDataHeader))
	{
		//(ctx, MSG_RESP_ERR);
		return;
	}

	// 3. 解析序号
	UploadDataHeader dataHdr;
	memcpy(&dataHdr, payload, sizeof(dataHdr));
	uint32_t seq = ntohl(dataHdr.seq);

	// 5. 提取文件数据
	const char* fileData = payload + sizeof(UploadDataHeader);
	uint32_t fileDataLen = payloadLen - sizeof(UploadDataHeader);

	// 检查分片是否越界
	uint64_t writeOffset = (uint64_t)seq * CHUNKSIZE;
	if (writeOffset + fileDataLen > ctx.uploadTotalSize) {
		WriteLog("[UPLOAD_DATA] sock=%llu OUT OF BOUNDS seq=%u offset=%llu size=%u total=%llu",
			(unsigned long long)ctx.sock, seq, (unsigned long long)writeOffset,
			fileDataLen, (unsigned long long)ctx.uploadTotalSize);
		return;
	}

	WriteLog("[UPLOAD_DATA] sock=%llu file=%S seq=%u dataSize=%u offset=%llu",
		(unsigned long long)ctx.sock, ctx.uploadFileName.c_str(),
		seq, fileDataLen, (unsigned long long)writeOffset);
	printf("[%s] [RECV UPLOAD_DATA] seq=%u dataSize=%u\n",
		GetDateTimeStr().c_str(), seq, fileDataLen);

	LARGE_INTEGER offset;
	offset.QuadPart = (LONGLONG)writeOffset; // 计算当前分片的偏移位置

	// 移动文件指针到正确位置
	SetFilePointerEx(ctx.uploadFileHandle, offset, NULL, FILE_BEGIN);

	// 6. 写入文件（你的项目风格是用 WriteFile） 
	DWORD written = 0;
	if (!WriteFile(ctx.uploadFileHandle, fileData, fileDataLen, &written, nullptr))
	{
		//PushResponse(ctx, MSG_RESP_ERR);
		return;
	}

	// 7. 更新状态
	ctx.uploadReceived += written;

	// 8. 判断是否收完
	if (ctx.uploadReceived == ctx.uploadTotalSize)
	{
		CloseHandle(ctx.uploadFileHandle);
		ctx.uploadFileHandle = INVALID_HANDLE_VALUE;
		ctx.uploading = false;

		std::wstring fullPath = m_storageDir + L"\\" + ctx.uploadFileName;
		uint32_t actualCrc = CalculateFileCRC32(fullPath);
		if (actualCrc != ctx.expectedCrc) {
			WriteLog("[UPLOAD_DONE] sock=%llu file=%S size=%llu crc=FAIL(expected=%08X actual=%08X)",
				(unsigned long long)ctx.sock, ctx.uploadFileName.c_str(),
				(unsigned long long)ctx.uploadTotalSize, ctx.expectedCrc, actualCrc);
			DeleteFileW(fullPath.c_str());
			MsgHeader errHdr;
			errHdr.id = htons(MSG_RESP_ERR);
			errHdr.len = htonl(0);
			const uint8_t* pErr = reinterpret_cast<const uint8_t*>(&errHdr);
			ctx.sendBuf.insert(ctx.sendBuf.end(), pErr, pErr + sizeof(errHdr));
		} else {
			WriteLog("[UPLOAD_DONE] sock=%llu file=%S size=%llu crc=OK",
				(unsigned long long)ctx.sock, ctx.uploadFileName.c_str(),
				(unsigned long long)ctx.uploadTotalSize);
			MsgHeader okHdr;
			okHdr.id = htons(MSG_RESP_OK);
			okHdr.len = htonl(0);
			const uint8_t* pOk = reinterpret_cast<const uint8_t*>(&okHdr);
			ctx.sendBuf.insert(ctx.sendBuf.end(), pOk, pOk + sizeof(okHdr));
		}
	}
}

void Server::HandleRefreshReq(Task& task, ClientContext& ctx)
{
	std::lock_guard<std::mutex> ctxLk(ctx.mtx);

	// 验证客户端是否已登录
	if (!ctx.authed)
	{
		MsgHeader respHeader;
		respHeader.id = htons(MSG_RESP_ERR);
		respHeader.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&respHeader);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(respHeader));
		return;
	}

	std::vector<FileInfo> files;
	// 遍历存储目录获取文件列表
	for (const auto& entry : std::filesystem::directory_iterator(m_storageDir))
	{
		if (entry.is_regular_file())
		{
			FileInfo fi{};
			std::wstring wname = entry.path().filename().wstring();
			int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string name(len - 1, '\0');
			WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], len, nullptr, nullptr);

			strncpy_s(fi.fileName, name.c_str(), sizeof(fi.fileName) - 1);

			files.push_back(fi);
		}
	}

	// 构造成功回复包头
	MsgHeader respHeader;
	respHeader.id = htons(MSG_FILE_LIST_RESP);
	respHeader.len = htonl(static_cast<uint32_t>(files.size() * sizeof(FileInfo)));

	const uint8_t* hdrPtr = reinterpret_cast<const uint8_t*>(&respHeader);
	ctx.sendBuf.insert(ctx.sendBuf.end(), hdrPtr, hdrPtr + sizeof(respHeader));

	WriteLog("[FILE_LIST] sock=%llu user=%s count=%zu", (unsigned long long)ctx.sock, ctx.user.c_str(), files.size());
	printf("[%s] [FILE_LIST] sock=%llu user=%s count=%zu\n", GetDateTimeStr().c_str(), (unsigned long long)ctx.sock, ctx.user.c_str(), files.size());

	// 附加包体（文件列表数据）
	if (!files.empty())
	{
		const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(files.data());
		ctx.sendBuf.insert(ctx.sendBuf.end(), dataPtr, dataPtr + files.size() * sizeof(FileInfo));
	}
}

void Server::HandleDownloadReq(Task& task, ClientContext& ctx)
{
	std::lock_guard<std::mutex> ctxLk(ctx.mtx);

	if (!ctx.authed)
	{
		MsgHeader respHeader;
		respHeader.id = htons(MSG_RESP_ERR);
		respHeader.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&respHeader);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(respHeader));
		return;
	}

	// 解析下载请求
	if (task.payload.size() < sizeof(DownloadReq))
	{
		MsgHeader respHeader;
		respHeader.id = htons(MSG_RESP_ERR);
		respHeader.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&respHeader);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(respHeader));
		return;
	}

	DownloadReq req;
	memcpy(&req, task.payload.data(), sizeof(req));

	std::string rawName(req.fileName, strnlen(req.fileName, sizeof(req.fileName)));
	if (rawName.empty())
	{
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 3. 安全检查（防止路径穿越）
	std::wstring wFileName;
	int len = MultiByteToWideChar(CP_UTF8, 0, rawName.c_str(), -1, nullptr, 0);
	if (len > 0)
	{
		wFileName.resize(len - 1);
		MultiByteToWideChar(CP_UTF8, 0, rawName.c_str(), -1, &wFileName[0], len);
	}
	if (wFileName.empty() ||
		wFileName.find(L"/") != std::wstring::npos ||
		wFileName.find(L"\\") != std::wstring::npos ||
		wFileName == L"." || wFileName == L"..")
	{
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 构造完整的文件路径
	std::wstring fullPath = m_storageDir + L"\\" + wFileName;
	HANDLE hFile = CreateFileW(fullPath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

	// 获取文件大小
	uint64_t fileSize = FileSize64(hFile);
	if (fileSize == 0)
	{
		CloseHandle(hFile);
		MsgHeader hdr;
		hdr.id = htons(MSG_RESP_ERR);
		hdr.len = htonl(0);
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
		ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
		return;
	}

        // 计算文件CRC32用于校验
        uint32_t fileCrc32 = CalculateFileCRC32(fullPath);

        // 回复成功响应（包含文件大小和CRC32）
	MsgHeader hdr;
	hdr.id = htons(MSG_RESP_OK);
	hdr.len = htonl(sizeof(DownloadRsp));
	const uint8_t* p = reinterpret_cast<const uint8_t*>(&hdr);
	ctx.sendBuf.insert(ctx.sendBuf.end(), p, p + sizeof(hdr));
	DownloadRsp rsp;
	rsp.fileSize = htonll(fileSize);
	rsp.crc32 = htonl(fileCrc32);
	const uint8_t* pRsp = reinterpret_cast<const uint8_t*>(&rsp);
	ctx.sendBuf.insert(ctx.sendBuf.end(), pRsp, pRsp + sizeof(rsp));

	// 设置下载状态，排队发送第一个分片
	ctx.downloading = true;
	ctx.downloadFileName = wFileName;
	ctx.downloadTotalSize = fileSize;
	ctx.downloadSent = 0;
	ctx.downloadSeq = 0;
	ctx.downloadFileHandle = hFile;

	WriteLog("[DOWNLOAD_REQ] sock=%llu file=%s size=%llu crc=%08X",
		(unsigned long long)ctx.sock, rawName.c_str(), (unsigned long long)fileSize, fileCrc32);
	printf("[%s] [RECV DOWNLOAD_REQ] file=%s size=%llu crc=%08X\n",
		GetDateTimeStr().c_str(), rawName.c_str(), (unsigned long long)fileSize, fileCrc32);

	QueueNextDownloadChunk(ctx);
}

void Server::QueueNextDownloadChunk(ClientContext& ctx)
{
	if (!ctx.downloading || ctx.downloadFileHandle == INVALID_HANDLE_VALUE)
		return;

	uint64_t remaining = ctx.downloadTotalSize - ctx.downloadSent;
	if (remaining == 0)
	{
		CloseHandle(ctx.downloadFileHandle);
		ctx.downloadFileHandle = INVALID_HANDLE_VALUE;
		ctx.downloading = false;
		return;
	}

	uint32_t curChunkSize = static_cast<uint32_t>(std::min<uint64_t>(CHUNKSIZE, remaining));
	DWORD bytesRead = 0;
	uint64_t offset = ctx.downloadSent;

	LARGE_INTEGER li;
	li.QuadPart = static_cast<LONGLONG>(offset);
	if (!SetFilePointerEx(ctx.downloadFileHandle, li, nullptr, FILE_BEGIN))
	{
		CloseHandle(ctx.downloadFileHandle);
		ctx.downloadFileHandle = INVALID_HANDLE_VALUE;
		ctx.downloading = false;
		return;
	}

	std::vector<uint8_t> readBuf(curChunkSize);
	if (!ReadFile(ctx.downloadFileHandle, readBuf.data(), curChunkSize, &bytesRead, nullptr) || bytesRead != curChunkSize)
	{
		CloseHandle(ctx.downloadFileHandle);
		ctx.downloadFileHandle = INVALID_HANDLE_VALUE;
		ctx.downloading = false;
		return;
	}

	DownloadDataHeader dataHdr;
	dataHdr.seq = htonl(ctx.downloadSeq);

	MsgHeader dataMsg;
	dataMsg.id = htons(MSG_DOWNLOAD_DATA);
	dataMsg.len = htonl(sizeof(dataHdr) + curChunkSize);

	const uint8_t* pMsg = reinterpret_cast<const uint8_t*>(&dataMsg);
	ctx.sendBuf.insert(ctx.sendBuf.end(), pMsg, pMsg + sizeof(dataMsg));
	const uint8_t* pHdr = reinterpret_cast<const uint8_t*>(&dataHdr);
	ctx.sendBuf.insert(ctx.sendBuf.end(), pHdr, pHdr + sizeof(dataHdr));
	ctx.sendBuf.insert(ctx.sendBuf.end(), readBuf.data(), readBuf.data() + curChunkSize);

	WriteLog("[DOWNLOAD_DATA] sock=%llu file=%S seq=%u dataSize=%u offset=%llu/%llu",
		(unsigned long long)ctx.sock, ctx.downloadFileName.c_str(),
		ctx.downloadSeq, curChunkSize, (unsigned long long)ctx.downloadSent,
		(unsigned long long)ctx.downloadTotalSize);
	printf("[%s] [SEND DOWNLOAD_DATA] seq=%u dataSize=%u\n",
		GetDateTimeStr().c_str(), ctx.downloadSeq, curChunkSize);

	ctx.downloadSent += curChunkSize;
	ctx.downloadSeq++;

	if (ctx.downloadSent >= ctx.downloadTotalSize) {
		CloseHandle(ctx.downloadFileHandle);
		ctx.downloadFileHandle = INVALID_HANDLE_VALUE;
		ctx.downloading = false;
		WriteLog("[DOWNLOAD_DONE] sock=%llu file=%S size=%llu",
			(unsigned long long)ctx.sock, ctx.downloadFileName.c_str(),
			(unsigned long long)ctx.downloadTotalSize);
	}
}

void Server::WriteLog(const char* fmt, ...)
{
	char buf[4096];
	std::snprintf(buf, sizeof(buf), "[%s] ", GetDateTimeStr().c_str());
	size_t prefixLen = strlen(buf);

	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf + prefixLen, sizeof(buf) - prefixLen, fmt, args);
	va_end(args);

	std::lock_guard<std::mutex> lk(m_logMtx);

	std::wstring logDir = L"logs";
	CreateDirectoryW(logDir.c_str(), nullptr);
	std::string dateStr = GetDateStr();
	std::wstring wDate(dateStr.begin(), dateStr.end());
	std::wstring path = logDir + L"\\" + wDate + L".log";

	std::ofstream ofs(std::filesystem::path(path), std::ios::app);
	if (ofs.is_open()) {
		ofs << buf << std::endl;
	}
}