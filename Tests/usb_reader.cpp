#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cassert>

using namespace std;

enum {CC=0, LenL=1, LenH=2};
class CommandBuilder
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    CommandBuilder() : length(LenH+1)
    {
        buffer[CC]=0x10;
    }

    void append(const string& s)
    {
        return append(s.c_str(), s.length());
    }

   void append(const char* s, size_t length_s)
    {
        assert(length + strlen(s) < BUF_SIZE);
            strcpy(buffer + length, s);
            length += strlen(s);
    }

    template<typename T>
    void append_var(T var)
    {
        assert(length+sizeof(var)<BUF_SIZE);
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
        buffer[LenL]+=sizeof(var);///TODO!
    }

    void write(ostream dev)
    {
        uint8_t sum=0;
        for_each(buffer+CC+1, buffer+length, [&sum](char c){sum+=c;});
        buffer[length]=sum;
        length+=sizeof(sum);
        dev.write(buffer, length);
        dev.flush();
    }
};

class CommandReader
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length{};
public:
    CommandReader() : length(0)
    {}

    char* read(istream dev)
    {
        dev.read(buffer, length = LenH+1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint16_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint16_t*>(buffer + LenL);
        assert(n<BUF_SIZE);
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        dev.read(buffer + length, 1); /// SS
        length+=1;
        uint8_t sum = 0;
        for_each(buffer + LenL, buffer + length - 1, [&sum](char c)
        { sum += c; });
        if (sum + 42 != buffer[length - 1])
        {
            cerr << "SS" << endl;
            assert(false);
        }
        return buffer;
    }
};

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
    m_file<<(double)(sum)/count<<endl;
    cout<<(double)(sum)/count<<endl;
    return 0;
}

void wait()
{
    getchar();
    if_exit=true;
}
