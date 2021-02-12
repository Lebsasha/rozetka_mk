#include <iostream>
#include <fstream>
#include <cstring>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
using namespace std;
int calc_mean(int i);
int main (int argc, char** argv)
{
    ofstream stat;
    if(strtol(argv[1], nullptr,10)<=10||strtol(argv[1], nullptr,10)>=1000)
    {
        cout <<"Wrong freq"<<endl;
        return 1;
    }
    if(argc==2)
    stat=ofstream("stat_"+string(argv[1])+".csv", std::ios_base::out | std::ios_base::app);
    else if (argc ==3)
    {
        return calc_mean(strtol(argv[1], nullptr, 10));
    }
    else
    {
        cout<<"Set frequency";
        return 1;
    }
    const char* command=(string("first onm ")+argv[1]).c_str();
    cout<<command<<endl;
    char time[64]={'0'};
    uint32_t speed;
    ofstream dev("/dev/ttyACM0");
    ifstream read_time("/dev/ttyACM0");
    if(!dev)
    {
        std::cout<<"Error opening COM file"<<std::endl;
        return 1;
    }
    cout<<"begin ";
    for(size_t i =0;i<1000;++i)
    {
        dev.write(command, strlen(command));
        dev.flush();
        read_time.read(time, sizeof(uint32_t));
        if(!read_time)
    {
        std::cout<<endl<<"Error reading COM file in "<<i<<" iteration"<<std::endl;
        return 1;
    }   
        speed = *reinterpret_cast<uint32_t*>(time);
        stat << speed << endl;
    }
    cout<<"end"<<endl;
    calc_mean(strtol(argv[1], nullptr, 10));
}

int calc_mean(int i)
{
    ifstream file("stat_"+to_string(i)+".csv");
    int count=0;
    int num;
    int sum=0;
    while (file)
    {
        file>>num;
        ++count;
        sum+=num;
    }
    ofstream m_file("stat_"+to_string(i)+"_m.csv");
    m_file<<(double)(sum)/count<<' '<<(double)(sum)/count*i<<endl;
    cout<<(double)(sum)/count<<' '<<(double)(sum)/count*i<<endl;
    return 0;
}
#pragma clang diagnostic pop
