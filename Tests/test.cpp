#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
//#include <filesystem>
#include <iostream>

int main()
{
//    system("echo \"1\" > /dev/ttyACM0");
//    system("sleep 0.5");
//    system("echo \"0\" > /dev/ttyACM0");
//    system("sleep 0.5");

#define usb_ins 1
    //std::filesystem::path dev("/dev/ttyACM0");
    //if(std::filesystem::is_character_file(dev))
    if(usb_ins)
    {
        //./measure.out
        std::cout<<"Begin\n";
        system("./measure.out 1000000 8");
        std::cout<<"16 will beg\n";
        system("sleep 10");
        std::cout<<"16 beg\n";
        system("./measure.out 1000000 16");
        std::cout<<"32 will beg\n";
        system("sleep 10");
        std::cout<<"32 beg"<<std::endl;
        system("./measure.out 1000000 32");
        std::cout<<"64 will beg\n";
        system("sleep 10");
        std::cout<<"64 beg"<<std::endl;
        system("./measure.out 1000000 64");
        std::cout<<"128 will beg\n";
        system("sleep 10");
        std::cout<<"128 beg"<<std::endl;
        system("./measure.out 1000000 128");
        std::cout<<"256 will beg\n";
        system("sleep 10");
        std::cout<<"256 beg"<<std::endl;
        system("./measure.out 1000000 256");
        std::cout<<"512 will beg\n";
        system("sleep 10");
        std::cout<<"512 beg"<<std::endl;
        system("./measure.out 100000 512");
    }
    std::cout<<"End"<<std::endl;
}
