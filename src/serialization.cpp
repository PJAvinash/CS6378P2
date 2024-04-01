#ifndef _SERIALIZATION_CPP
#define _SERIALIZATION_CPP
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include "serialization.h"

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

template <typename T1>
std::vector<unsigned char> vectobytes(const std::vector<T1> &data){
    std::vector<unsigned char> buffer(sizeof(T1) * data.size());
    unsigned char* ptr = buffer.data();
    for (const auto& item : data) {
        std::memcpy(ptr, &item, sizeof(T1));
        ptr += sizeof(T1);
    }
    return buffer;
}

template <typename T1>
std::vector<T1> bytestovec(const std::vector<unsigned char> &buffer){
    std::vector<T1> rv;
    size_t numobjects = buffer.size() / sizeof(T1);
    rv.reserve(numobjects);
    const unsigned char* ptr = buffer.data();
    for(size_t i = 0; i< numobjects; i++){
        T1 obj;
        std::memcpy(&obj, ptr, sizeof(T1));
        rv.push_back(obj);
        ptr += sizeof(T1);
    }
    return rv;
}
#endif

