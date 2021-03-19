#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>

using namespace std;

volatile bool if_exit=false;

int calc_mean(const string& path);
void wait();
int main (int argc, char** argv)
{
    const char* path="react_time.csv";
    ofstream stat(path, ios_base::app|ios_base::out);
    const char* command="measure ";
    char comp_command[]="sleep  ";
    char time_buffer[64]={'0'};
    uint32_t speed;
    ofstream dev("/dev/ttyACM0");
    ifstream read_time("/dev/ttyACM0");
    if(!dev || !read_time)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    std::thread waiter(wait);
    cout<<"begin ";///TODO Сделать "прозвон"
    for(size_t i =0;i<7;++i)
    {
        comp_command[sizeof(comp_command) - 1 - 1]= rand() % 6 + '0';
        system(comp_command);
        dev.write(command, strlen(command));
        dev.flush();
        read_time.read(time_buffer, sizeof(uint32_t));
        if(!read_time)
    {
        std::cout<<endl<<"Error reading COM file in "<<i<<" iteration"<<std::endl;
        return 2;
    }
        speed = *reinterpret_cast<uint32_t*>(time_buffer);
        std::cout<<i<<' '<<speed<<endl;
        stat << speed << endl;
        if(if_exit)
            break;
    }
    cout<<"end"<<endl;
    calc_mean(path);
}

int calc_mean(const string& path)
{
    ifstream file(path);
    int count=0;
    int num;
    int sum=0;
    while (file)
    {
        file>>num;
        ++count;
        sum+=num;
    }
    ofstream m_file("stat_" + path);
    m_file<<(double)(sum)/count<<' '<<(double)(sum)/count<<endl;
    cout<<(double)(sum)/count<<' '<<(double)(sum)/count<<endl;
    return 0;
}

void wait()
{
    getchar();
    if_exit=true;
}
