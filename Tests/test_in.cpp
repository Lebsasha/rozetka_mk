#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int process_one_cmd(int packets, int size_of_packet);

int main(int argc, char** argv)
{
    int packets = 10000;
    int size_of_packet = 8;
    std::vector<size_t> sizes;
    if (argc == 3)
    {
        packets = atoi(argv[1]);
        size_of_packet = atoi(argv[2]);
        return process_one_cmd(packets, size_of_packet);
    }
    if (argc == 2 || argc >= 4)
    {
        for (size_t i = 1; i < argc; ++i)
        {
            if(*(argv[i])=='0')
                break;
            sizes.push_back(strtol(argv[i], nullptr, 10));
            if (errno == ERANGE)
                return 1;
        }
    }
    for (int j = 0; j < 10; ++j)
        for (size_t i: sizes)
        {
            std::cout << "Begin " << i << std::endl;
            if (!process_one_cmd(packets, i))
            {
                std::cerr << "Bad time in " << i << std::endl;
            }
        }
    return 0;
}

int process_one_cmd(int packets, int size_of_packet)
{
    std::ifstream dev("/dev/ttyACM0");
    std::ofstream send_cmd("/dev/ttyACM0");
    if (!dev)
    {
        std::cerr << "Error opening file" << std::endl;
        exit(1);
    }
    if (!send_cmd)
    {
        std::cerr << "Error opening file" << std::endl;
        exit(1);
    }
    std::string cmd = "211 " + std::to_string(packets) + ' ' + std::to_string(size_of_packet) + ' ';
    char* s = new char[packets * size_of_packet + 100UL];
    send_cmd.write(cmd.c_str(), cmd.length());
    send_cmd.flush();
    dev.read(s, packets * size_of_packet + sizeof("\n end") + sizeof(".") - 1 + sizeof(uint32_t));
    uint32_t time = *reinterpret_cast<uint32_t*>(s + packets * size_of_packet + sizeof("\n end") + sizeof(".") - 1);
    std::ofstream log("res_in_new.csv", std::ios_base::out | std::ios_base::app);
    log << size_of_packet << ", " << static_cast<double>(packets * size_of_packet) * 100'000 / time << ", "
        << static_cast<double>(time) / 100'000 << ", "
                                                  "" << packets << std::endl;
    dev.close();
    send_cmd.close();
    log.close();
    return time;
}
