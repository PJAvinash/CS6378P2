// #include "../serialization.h"
// #include "../dsstructs.h"
#include <vector>


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
    for(int i = 0; i< numobjects; i++){
        T1 obj;
        std::memcpy(&obj, ptr, sizeof(T1));
        rv.push_back(obj);
        ptr += sizeof(T1);
    }
    return rv;
}

template <typename T1,typename T2>
struct Message
{
    int from;
    T1 key;
    T2 value;
};


int main() {
    std::vector<Message <int,int> > v;
    v.push_back(Message <int,int>{1,2,3});
    v.push_back(Message <int,int>{3,4,6});
    std::vector<unsigned char> bytes = vectobytes(v);
    std::vector<Message <int,int> > v2 = bytestovec< Message <int,int> >(bytes);
    for( const Message<int ,int> c:v2){
        printf("%d,%d,%d \n",c.from,c.key,c.value);
    }


    // std::vector<unsigned char> a = tobytes(vecOfVecs);
    // auto b = frombytes <std::vector< std::vector<unsigned char> >> (a);
    // for(int i = 0; i<b.size(); i++){
    //     for(int j = 0; j < b[i].size(); j++){
    //         if(b[i][j] != vecOfVecs[i][j]) {
    //             printf("mismatch\n");
    //         }
    //     }
    // }
}