#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <vector>
#include <cstring>
#include <stdexcept>
template <typename T1>
std::vector<unsigned char> tobytes(const T1 &data);

template <typename T1>
T1 frombytes(const std::vector<unsigned char> &v);
#endif // SERIALIZATION_H
