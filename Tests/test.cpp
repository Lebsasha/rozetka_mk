#include <fstream>
#include <iostream>
int main()
{
#define usb_ins 1
    std::ofstream dev("/dev/ttyACM0");
    if(!dev)
    {
        std::cout<<"Error"<<std::endl;
        return 0;
    }
    if(usb_ins)
    for (int i=0; i <100000; ++i)
    {
        dev.write("0123456789012345678901234567890123456789012345678901234567890123", 64);// "0123456789012345678901234567890123456789012345678901234567890123";
        dev.flush();
    }
}
