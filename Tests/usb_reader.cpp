#include <iostream>
#include <fstream>
#include <cstring>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
using namespace std;
int calc_mean(const string& i);
void wait();
int main (int argc, char** argv)
{
    const char* path="react_time.csv";
    ofstream stat(path, ios_base::ate|ios_base::out);
    const char* command="measure ";
    char sleep_time[]="sleep  ";
    char time[64]={'0'};
    uint32_t speed;
    ofstream dev("/dev/ttyACM0");
    ifstream read_time("/dev/ttyACM0");
    if(!dev)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    cout<<"begin ";
    for(size_t i =0;i<15;++i)
    {
        sleep_time[sizeof(sleep_time)-1-1]=rand()%10+'0';
        system(sleep_time);
        dev.write(command, strlen(command));
        dev.flush();
        read_time.read(time, sizeof(uint32_t));
        if(!read_time)
    {
        std::cout<<endl<<"Error reading COM file in "<<i<<" iteration"<<std::endl;
        return 1;
    }   std::cout<<i<<endl;
        speed = *reinterpret_cast<uint32_t*>(time);
        stat << speed << endl;
    }
    cout<<"end"<<endl;
    calc_mean(path);
}

int calc_mean(const string& i)
{
    ifstream file(i);
    int count=0;
    int num;
    int sum=0;
    while (file)
    {
        file>>num;
        ++count;
        sum+=num;
    }
    ofstream m_file("stat_"+i);
    m_file<<(double)(sum)/count<<' '<<(double)(sum)/count<<endl;
    cout<<(double)(sum)/count<<' '<<(double)(sum)/count<<endl;
    return 0;
}

void wait()
{
    int i;
    cin>>i;
    exit(1);
}
#pragma clang diagnostic pop
