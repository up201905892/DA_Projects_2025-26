#include<iostream>
#include<cstdlib>
#include<fstream>
int main(){
  const char* t=std::getenv("TEMP");
  std::cout<<"TEMP="<<(t?t:"(null)")<<"\n";
  if(t){
    std::string path=std::string(t)+"/mytest.txt";
    std::cout<<"Writing to: "<<path<<"\n";
    std::ofstream f(path);
    if(!f.is_open()){std::cerr<<"FAIL\n";return 1;}
    f<<"hello";f.close();
    std::cout<<"OK\n";
  }
  return 0;
}
