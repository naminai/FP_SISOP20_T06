#include "types.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "user.h"


int main (int argc, char *argv[])
{
	int fd1, fd2, byte, line;
	char t1, t2;
    	if(argc != 3)
    	{
		printf (1, "Penggunaan: cmp [file1] [file2]\n");
        	exit();
    	}
	if((fd1 = open(argv[1], O_RDONLY)) < 0)
    	{
      		printf(1, "%s tidak dapat ditemukan\n", argv[1]);
        	exit();
    	}

    	else if((fd2 = open(argv[2], O_RDONLY)) < 0)
    	{	
        	printf(1, "%s tidak dapat ditemukan\n", argv[2]);
        	exit();
    	}
	else if((fd2 = open(argv[2], O_RDONLY)) < 0 || (fd1 = open(argv[1], O_RDONLY)) < 0)
    	{	
        	printf(1, "%s dan %s tidak dapat ditemukan\n", argv[1], argv[2]);
        	exit();
    	}
	else
	{
		byte = 0;
		line = 1;
		while (1)
		{
        		if((read(fd1, &t1, sizeof(t1))) <= 0 || (read(fd2, &t2, sizeof(t2))) <= 0)
			{
				break;
			}
			if(t1 == '\n' && t2 == '\n')
			{
				++line;
			}
        		if(t1 != t2)
			{
            	break;
        	}
			++byte;
		}

		if(read(fd1, &t1, sizeof(t1))!=0 || read(fd2, &t2, sizeof(t2))!=0)
    		{
	 			printf(1, "%s dan %s memiliki perbedaan pada: byte %d line %d\n", argv[1], argv[2], byte, line);
				exit();
    		}
		else
		{
			printf(1, "Tidak ada perbedaan\n");
			exit();
		}
		close(fd1);
    	close(fd2);
        return 0;
	}
    exit();
	
}