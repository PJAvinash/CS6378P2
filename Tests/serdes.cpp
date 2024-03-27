#include "../serialization.h"
#include "../dsstructs.h"
#include <vector>


int main() {
    //  std::vector< Message <std::vector<int> , std::vector<int> > > vecOfVecs;

    // // Initialize each inner vector separately
    // for (int i = 0; i < 3; ++i) {
    //     Message <std::vector<int> , std::vector<int> > m(1,std::vector<int>{1,2,3,4},std::vector<int>{4,5,6,7});
        
    // }


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