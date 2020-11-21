#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <filesystem>
#include <iostream>

int main()
{
//    system("echo \"1\" > /dev/ttyACM0");
//    system("sleep 0.5");
//    system("echo \"0\" > /dev/ttyACM0");
//    system("sleep 0.5");

#define usb_ins 1
    std::filesystem::path dev("/dev/ttyACM0");
    if(std::filesystem::is_character_file(dev))
    if(usb_ins)
    {
        //./measure.out
        std::cout<<"Begin\n";
        system("echo 1000000 8");
        std::cout<<"16 beg\n";
        system("echo 1000000 16");
        std::cout<<"32 beg"<<std::endl;
        system("echo 1000000 32");
        std::cout<<"64 beg"<<std::endl;
        system("echo 1000000 64");
        std::cout<<"128 beg"<<std::endl;
        system("echo 1000000 128");
        std::cout<<"256 beg"<<std::endl;
        system("echo 1000000 256");
        std::cout<<"512 beg"<<std::endl;
        system("echo 100000 512");
    }
    std::cout<<"End"<<std::endl;
}
str_cmp(u_int8_t * buf, char* str, size_t n)
{
    ++n;
    while (--n)
    {
        if(*buf++!=*str++)
            return 1;
    }
    return 0;fdfdfdfdf
}