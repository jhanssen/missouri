#ifndef HOST_H
#define HOST_H

#include <cstdint>
#include <string>

class Host
{
public:
    Host(const std::string& addr, uint16_t port);

    bool isValid() const { return mData != 0; }

    uint32_t address() const { return mData & 0x00000000FFFFFFFF; }
    uint16_t port() const { return mData >> 48; }

private:
    uint64_t mData;
};

#endif
