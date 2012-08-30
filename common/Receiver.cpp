#include "Receiver.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

Receiver::Receiver()
{
    mCurrent.data = static_cast<char*>(malloc(4));
    mCurrent.size = mCurrentSize = 0;
}

Receiver::~Receiver()
{
    free(mCurrent.data);
    std::deque<Block>::const_iterator it = mBlocks.begin();
    const std::deque<Block>::const_iterator end = mBlocks.end();
    while (it != end) {
        free(it->data);
        ++it;
    }
}

void Receiver::feed(const char* data, int size)
{
    do {
        assert(size > 0);
        if (!mCurrent.size) { // we don't yet know the total size for this block
            if (mCurrentSize + size >= 4) { // we have a complete size
                if (!mCurrentSize) { // optimize for the common case
                    mCurrent.size = htonl(*reinterpret_cast<const int*>(data));
                    mCurrent.data = static_cast<char*>(realloc(mCurrent.data, mCurrent.size));
                    data += 4;
                    size -= 4;
                } else {
                    assert(mCurrentSize < 4);
                    const int diff = 4 - mCurrentSize;
                    assert(diff <= size);
                    memcpy(mCurrent.data + mCurrentSize, data, diff);
                    mCurrentSize = 0;
                    mCurrent.size = htonl(*reinterpret_cast<const int*>(mCurrent.data));
                    mCurrent.data = static_cast<char*>(realloc(mCurrent.data, mCurrent.size));
                    data += diff;
                    size -= diff;
                }
            } else { // we don't have enough data for a complete size
                memcpy(mCurrent.data + mCurrentSize, data, size);
                mCurrentSize += size;
            }
        }
        if (size > 0 && mCurrent.size) { // we have some data
            const int diff = size - mCurrentSize;
            assert(diff <= size);
            memcpy(mCurrent.data + mCurrentSize, data, diff);
            data += diff;
            size -= diff;
            if (mCurrentSize + size >= mCurrent.size) { // we have the entire data
                mCurrentSize = 0;
                mBlocks.push_back(mCurrent);
                mCurrent.size = 0;
                mCurrent.data = static_cast<char*>(malloc(4));
            } else { // we don't have the entire data
                mCurrentSize += size;
            }
        }
    } while (size);
}

bool Receiver::hasBlock() const
{
    return !mBlocks.empty();
}

bool Receiver::popBlock(char** data, int* size)
{
    if (mBlocks.empty())
        return false;

    Block& block = mBlocks.front();
    *data = block.data;
    *size = block.size;
    mBlocks.pop_front();
    return true;
}
