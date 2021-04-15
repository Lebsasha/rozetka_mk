#include <iostream>
#include <fstream>
#include <cstring>
//#include <thread>
#include <algorithm>
#include <cassert>

using namespace std;

///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum
{
    CC = 0, LenL = 1, LenH = 2
};

//enum Commands{Set_tone};
/**
 * 0x1 -> u8|version u8[]|"string with \0"
 * 0x4 ->
 * 0x10 u8|port u8|size u16[1-3]|freq ->
 * 0x11 u8|port ->
 * 0x12 -> u16|react_time//TODO this line
 */
uint8_t Commands[]={0x1, 0x4, 0x10, 0x11, 0x12};

static const uint8_t SS_OFFSET = 42;
static const size_t BUF_SIZE = 64;///USB packet size

class CommandWriter
{
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    CommandWriter() : length(LenH + 1)
    {}

    template<typename T>
    void append_var(T var)
    {
        assert(length + sizeof(var) < BUF_SIZE);///Less only (without equal) because we must append CC
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
        buffer[LenL] += sizeof(var);
    }

    void write(ostream& dev)
    {
        dev.write(buffer, length);
        dev.flush();
        for(char* c=buffer+length-1;c>=buffer;--c)
        {
            *c=0;
        }
        length=LenH+1;
//        this->CommandWriter::~CommandWriter();
//        *this=CommandWriter();
    }

    void prepare_for_sending()
    {
        assert(buffer[CC]!=0);
        uint8_t sum = SS_OFFSET;
        for_each(buffer + CC + 1, buffer + length, [&sum](char c)
        { sum += c; });
        buffer[length] = sum;
        length += sizeof(sum);
    }

    void set_cmd(const uint8_t cmd)
    {
        assert(count(Commands, Commands + sizeof(Commands)/sizeof(uint8_t), cmd)==1);
        buffer[CC] = cmd;
    }
};

class CommandReader
{
    char buffer[BUF_SIZE] = {0};
    size_t length;///TODO Проконтролировать на =0 везде
    size_t read_length;
public:
    CommandReader() : length(0), read_length(0)
    {}

    char* read(istream& dev)
    {
        dev.read(buffer, LenH + 1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint8_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint8_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        length =LenH+1;
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        dev.read(buffer + length, sizeof (uint8_t)); /// SS
        uint8_t sum = SS_OFFSET;
        for_each(buffer + LenL, buffer + length, [&sum](char c)
        { sum += c; });
        if (sum /*+ SS_OFFSET*/ != (uint8_t) buffer[length])
        {
            cerr << "SS isn't correct" << endl;
            assert(false);
        }
        length += sizeof (uint8_t);
        read_length= LenH + 1;
        return buffer;
    }

    bool is_empty() const
    {
        return length==3+1 || length==0;
    }
    int is_error() const
    {
        return buffer[CC]&(1<<7);
    }
    template<typename T>
    T get_param(T& param)
    {
       assert(length!=0);
        assert(read_length+sizeof(T)+1<=length);///TODO Mrite the same for mk
       param = *reinterpret_cast<T*>(buffer + read_length);
       read_length+=sizeof(T);
        return param;
    }

    template<typename T>
    T get_param()
    {
       T param;
        return get_param(param);
    }
    uint8_t get_command(uint8_t& cmd) const
    {
        return cmd=buffer[CC];
    }
};

//volatile bool if_exit=false;

int calc_mean(const string& path);
//void wait();
int main (int , char** )
{
    const char* path="react_time.csv";
    ofstream stat(path, ios_base::app|ios_base::out);
    CommandWriter command;
    CommandReader reader;
    char comp_command[]="sleep  ";
    uint8_t command_rec; ///Command received
    ofstream dev("/dev/ttyACM0");
    ifstream dev_read("/dev/ttyACM0");
    if(!dev || !dev_read)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
//    std::thread waiter(wait);
    cout<<"begin "<<std::flush;///TODO Сделать "прозвон"
    for(size_t i =0;i<4;++i)
    {
        comp_command[sizeof(comp_command) - 1 - 1] = rand() % 5 + '0';
        system(comp_command);
        const int cmd = 0x11;
        command.set_cmd(cmd);
        command.append_var<uint8_t>(0);/// Port
        command.prepare_for_sending();
        command.write(dev);
        reader.read(dev_read);
        assert(!reader.is_error());
        assert(reader.get_command(command_rec) == cmd);
        assert(reader.is_empty());
        do
        {
            system("sleep 1");
            command.set_cmd(0x12);
            command.prepare_for_sending();
            command.write(dev);
            reader.read(dev_read);
        } while (reader.is_error());
        uint16_t react_time;
        std::cout << i << ' ' << reader.get_param(react_time)<<endl;
//        stat << speed << endl;
//        if(if_exit)
//            break;
    }

    cout<<"end"<<endl;
//    if(if_exit)
//        waiter.join();
//   waiter.std::thread::~thread();
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

//void wait()
//{
//    getchar();
//    if_exit=true;
//}
