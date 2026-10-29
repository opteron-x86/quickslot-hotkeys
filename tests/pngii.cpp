#include "pngii_checks.h"
int main()try{pngii_test::checks();}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
