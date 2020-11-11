#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
int str_cmp(u_int8_t * buf, char* str, size_t n)
{
    ++n;
    while (--n)
    {
        if(*buf++!=*str++)
            return 1;
    }
    return 0;
}
main()
{
//    system("echo \"1\" > /dev/ttyACM0");
//    system("sleep 0.5");
//    system("echo \"0\" > /dev/ttyACM0");
//    system("sleep 0.5");
#define usb_ins 1
    assert(str_cmp("ss", "ss",2)==0);
    assert(str_cmp("as", "ss",2)==1);
    assert(str_cmp("s ", "ss",2)==1);
    assert(str_cmp("sa", "ss",2)==1);
    assert(str_cmp("sa", "ss",2)==1);
    assert(str_cmp("sasas", "ss",2)==1);
    assert(str_cmp("sasas", "sasa",4)==0);
    assert(str_cmp("sasas", "sasaa",5)==1);
    assert(str_cmp("sasas", "sasas",5)==0);
    assert(str_cmp("sasas", "sasas",sizeof("sasas"))==0);
    assert(str_cmp("sasas", "sasts",sizeof("sasas")-2-1)==0);
    assert(str_cmp("first on"+sizeof("first ")-1, "on", 2)==0);
    assert(strcmp("first on"+sizeof("first ")-1, "on")==0);
    assert(strcmp("first_on"+sizeof("first_")-1, "on")==0);
    u_int8_t* buf="first o";
    char* cmd=(char*)buf;
    if(strcmp(cmd, "first")>=0)//Fix triggering
    {
        if (strcmp(cmd + sizeof("first ") - 1, "o") == 0)
        {
            if(usb_ins)
                assert(0);
        }
    }
//    if(usb_ins)
//    for (int i=1; i <100; ++i)
//    {
//        system("echo \"0\" > /dev/ttyACM0");
//        system("sleep 0.0001");
//    }
}
