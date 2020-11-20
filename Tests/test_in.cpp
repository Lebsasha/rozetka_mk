#include <fstream>
#include <iostream>
#include <string>
int main()
{
#define usb_ins 1
    std::ifstream dev("/dev/ttyACM0");
    std::ofstream send_cmd("/dev/ttyACM0");
     int packets=1000;
     int size_of_packet=8;
    std::string cmd="211 "+std::to_string(packets)+' '+std::to_string(size_of_packet)+' ';
    if(!dev)
    {
        std::cout<<"Error"<<std::endl;
        return 1;
    }
    if(!send_cmd)
    {
        std::cout<<"Error"<<std::endl;
        return 1;
    }
    char s[packets*size_of_packet+100UL];
    send_cmd.write(cmd.c_str(), cmd.length());
    send_cmd.flush();
    //if(usb_ins)
    //for (int i=0; i <100000; ++i)
    //{
        dev.read(s, 100*size_of_packet);// "0123456789012345678901234567890123456789012345678901234567890123";
        //dev.flush();
    //}
    std::cout<<std::string(s, 1000*size_of_packet)<<std::endl;
}
