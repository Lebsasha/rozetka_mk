#include <stdlib.h>

main()
{
//    system("echo \"1\" > /dev/ttyACM0");
//    system("sleep 0.5");
//    system("echo \"0\" > /dev/ttyACM0");
//    system("sleep 0.5");
    for (int i=1; i <200; ++i)
    {
        system("echo \"0\" > /dev/ttyACM0");
        system("sleep 0.0001");
    }
}
