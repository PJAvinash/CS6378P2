#include <vector>
#include <cstring>
#include <stdexcept>
#include "serialization.h"
#include <iostream>

template <typename T1>
std::vector<unsigned char> tobytes(const T1& data) {
    std::vector<unsigned char> bytes(sizeof(T1));
    std::memcpy(bytes.data(), &data, sizeof(T1));
    return bytes;
}

template <typename T1>
T1 frombytes(const std::vector<unsigned char> &v)
{
    if (v.size() != sizeof(T1))
    {
        throw std::runtime_error("Input vector size does not match the size of the struct");
    }
    T1 result;
    std::memcpy(&result, v.data(), sizeof(T1));
    return result;
}
