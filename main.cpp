#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include "bcardi.h"



using namespace std;
using namespace chrono;





int main(int argc, char ** argv){
   
    srand(time(NULL));
    string  fof((argv[1]));
    Bcardi Richelieu(1,(uint64_t)10*1000*1000*1000,1000*1000*1000,1,31);
    Richelieu.insert_file_of_file(fof);
    Richelieu.get_cardinalities();
}