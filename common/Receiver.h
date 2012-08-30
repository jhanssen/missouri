#ifndef RECEIVER_H
#define RECEIVER_H

#include <deque>

class Receiver
{
public:
    Receiver();
    ~Receiver();

    void feed(const char* data, int size);

    bool hasBlock() const;
    bool popBlock(char** data, int* size);

private:
    struct Block {
        char* data;
        int size;
    };
    std::deque<Block> mBlocks;
    Block mCurrent;
    int mCurrentSize;
};

#endif
