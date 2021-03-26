#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cassert>

using namespace std;

///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum
{
    CC = 0, LenL = 1, LenH = 2
};

static const uint8_t SS_OFFSET = 42;

class CommandWriter
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    CommandWriter() : length(LenH + 1)
    {
        buffer[CC] = 0x10;///TODO enum
    }

    template<typename T>
    void append_var(T var)
    {
        assert(length + sizeof(var) < BUF_SIZE);
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
        buffer[LenL] += sizeof(var);///TODO!
    }

    void write(ostream& dev)
    {
        uint8_t sum = SS_OFFSET;
        for_each(buffer + CC + 1, buffer + length, [&sum](char c)
        { sum += c; });
        buffer[length] = sum;
        length += sizeof(sum);
        dev.write(buffer, length);
        dev.flush();
    }
};

class CommandReader
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length;
    size_t read_lehgth;
public:
    CommandReader() : length(0), read_lehgth(0)
    {}

    char* read(istream& dev)
    {
        dev.read(buffer, length = LenH + 1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint8_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint8_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        dev.read(buffer + length, sizeof (uint8_t)); /// SS
        length += sizeof (uint8_t);
        uint8_t sum = SS_OFFSET;
        for_each(buffer + LenL, buffer + length - 1, [&sum](char c)
        { sum += c; });
        if (sum /*+ SS_OFFSET*/ != (uint8_t) buffer[length - 1])///(weak TODO) Why?
        {
            cerr << "SS is't correct" << endl;
            assert(false);
        }
        read_lehgth=LenH+1;
        return buffer;
    }

    template<typename T>
    T get_param(T& param)
    {
       assert(length!=0);
       param = *reinterpret_cast<T*>(buffer+read_lehgth);
       read_lehgth+=sizeof(T);
        return param;
    }

    uint8_t get_command(uint8_t& cmd)
    {
        return cmd=buffer[CC];
    }
};

volatile bool if_exit=false;

int calc_mean(const string& path);
void wait();
int main (int argc, char** argv)
{
    const char* path="react_time.csv";
    ofstream stat(path, ios_base::app|ios_base::out);
    CommandWriter command;
    command.append_var<uint8_t>(0);/// Port TODO ?
    command.append_var<uint16_t>(500);///Freq
    CommandReader reader;
    char comp_command[]="sleep  ";
    uint8_t port_rec;
    uint16_t freq_rec;
    uint8_t command_rec;
    ofstream dev("temp.log");
    ifstream read_time("temp.log");
    if(!dev || !read_time)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    std::thread waiter(wait);
    cout<<"begin ";///TODO Сделать "прозвон"
    for(size_t i =0;i<7;++i)
    {
        comp_command[sizeof(comp_command) - 1 - 1]= '0';//rand() % 6 + '0';
        system(comp_command);
        command.write(dev);
       reader.read(read_time);
       reader.get_command(command_rec);
       reader.get_param(port_rec);
       reader.get_param(freq_rec);
        std::cout << i << ' ' << command_rec<<port_rec << freq_rec << endl;
//        stat << speed << endl;
        if(if_exit)
            break;
    }
    cout<<"end"<<endl;
    //calc_mean(path);
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
