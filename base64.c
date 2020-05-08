#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#define SIZE 1000

char* base64Encoder(char input_str[], int len_str)
{

    char char_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *res_str = (char *) malloc(SIZE * sizeof(char));

    int index, no_of_bits = 0, padding = 0, val = 0, count = 0, temp;
    int i, j, k = 0;

    for (i = 0; i < len_str; i += 3)
        {
            val = 0, count = 0, no_of_bits = 0;

            for (j = i; j < len_str && j <= i + 2; j++)
            {
                val = val << 8;
                val = val | input_str[j];
                count++;

            }

            no_of_bits = count * 8;
            padding = no_of_bits % 3;
            while (no_of_bits != 0)
            {
                if (no_of_bits >= 6)
                {
                    temp = no_of_bits - 6;

                    index = (val >> temp) & 63;
                    no_of_bits -= 6;
                }
                else
                {
                    temp = 6 - no_of_bits;
                    index = (val << temp) & 63;
                    no_of_bits = 0;
                }
                res_str[k++] = char_set[index];
            }
    }

    // padding 
    for (i = 1; i <= padding; i++)
    {
        res_str[k++] = '=';
    }

    res_str[k] = '\0';

    return res_str;

}

int main(int argc,char* argv[])
{
    char* test;
    int fd1;

    if(argc < 1)
    {
      printf(1,"Penggunaan: base64 filename\n");
      exit();
    }
    
    if((fd1 = open(argv[1],O_RDONLY)) < 0)
    {
      printf(1,"Error membuka file %s\n",argv[1]);
      exit();
    }
    char buff[1000];
    read(fd1,buff,1000);
    printf(1,"%s",buff);
    int i = 0;
    while(buff[i] != '\0')
    {
      i++;
    }

    test = base64Encoder(buff, i-1);
    printf(1,"%s\n", test);
    exit();
}