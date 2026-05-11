#include "const.h"
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <string>

static bool sendAll(SOCKET sock, const char* buf, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n == SOCKET_ERROR)
            return false;
        sent += n;
    }
    return true;
}

bool sendPacket(SOCKET sock, uint16_t id, const void* data, uint32_t len)
{
    MsgHeader hdr;
    hdr.id = htons(id);
    hdr.len = htonl(len);
    if (!sendAll(sock, (const char*)&hdr, sizeof(hdr)))
        return false;
    if (len > 0 && !sendAll(sock, (const char*)data, len))
        return false;
    return true;
}

bool recvPacket(SOCKET sock, uint16_t& out_id, std::vector<char>& out_data, std::vector<char>& recvBuf)
{
	while (true) {
		if (recvBuf.size() >= sizeof(MsgHeader)) {
			MsgHeader* hdr = reinterpret_cast<MsgHeader*>(recvBuf.data());
			uint32_t len = ntohl(hdr->len);
			if (len > FTA_MAX_PAYLOAD)
				return false;
			if (recvBuf.size() >= sizeof(MsgHeader) + len) {
                out_id = ntohs(hdr->id);
                out_data.assign(recvBuf.begin() + sizeof(MsgHeader),
                    recvBuf.begin() + sizeof(MsgHeader) + len);
                recvBuf.erase(recvBuf.begin(), recvBuf.begin() + sizeof(MsgHeader) + len);
                return true;
            }
        }

        char tmp[4096];
        int ret = recv(sock, tmp, sizeof(tmp), 0);
        if (ret <= 0) {
            return false;
        }
        recvBuf.insert(recvBuf.end(), tmp, tmp + ret);
    }
}

uint32_t CRC32::s_table[256];
std::once_flag CRC32::s_tableInit;

CRC32::CRC32()
    : m_crc(0xFFFFFFFF)
{
    std::call_once(s_tableInit, []() {
        for (int i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
            s_table[i] = c;
        }
    });
}

void CRC32::update(const void* data, size_t len)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i)
        m_crc = s_table[(m_crc ^ p[i]) & 0xFF] ^ (m_crc >> 8);
}

uint32_t CRC32::final()
{
    return m_crc ^ 0xFFFFFFFF;
}


