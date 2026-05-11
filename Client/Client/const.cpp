#include "const.h"
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <string>
#include <filesystem>
#include <system_error>

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

uint32_t calculateCRC32(const QByteArray& data)
{
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
            table[i] = c;
        }
        init = true;
    }

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}


