#include <fstream>
#include <iostream>
#include <string>
int main(int argc, char** argv)
{
#define usb_ins 1
    std::ifstream dev("/dev/ttyACM0");
    std::ofstream send_cmd("/dev/ttyACM0");
    int packets=1000;
    int size_of_packet=8;
    if(argc==3)
    {
        packets=atoi(argv[1]);
        size_of_packet=atoi(argv[2]);
    }
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
    char* s=new char[packets*size_of_packet+100UL];
    send_cmd.write(cmd.c_str(), cmd.length());
    send_cmd.flush();
    dev.read(s, packets*size_of_packet+sizeof("\n end")+sizeof(int));
    std::cout<<(cmd=std::string(s, packets*size_of_packet+sizeof("\n end")+sizeof(int)))<<std::endl;
    int time = *reinterpret_cast<int*>(s+packets*size_of_packet+sizeof("\n end"));
    //int time= (char)(ptr+3)*0xff000000+
    std::ofstream log("res.csv", std::ios_base::out|std::ios_base::app);
    log<<size_of_packet<<", "<< static_cast<double>(packets)*size_of_packet/time<<", "<<time<<", "<<packets<<std::endl;
}
