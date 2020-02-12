#include <stdio.h>

int main()
{
    printf("Hello World");
    
    unsigned short int a=0xFFFF;
    int cmd[6]={0x01, 0x04, 0x00, 0x06, 0x00, 0x02};
    int LSB;
    
    for (int i = 0; i < 6; i++)
    {
        a = a^cmd[i];
        for (int j = 0; j < 8; j++)
        {
            LSB = a & 0x0001;
            if (LSB == 1) 
            {
                a = a - 1;
            }
            a = a >> 1;
            if (LSB == 1)
            {
                a = a ^ 0xA001;
            }
        }
    }
    
    printf("a = %d", a);

    return 0;
}