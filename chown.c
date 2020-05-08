#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"
#include "fs.h"

// Sets the user UID for the target specified by pathname.
int main(int argc, char *argv[])
{
  if(argc > 2)
  {
    if(strcmp(argv[1], "root:user") == 0)
    {
      chown(argv[2], 0);
      chgrp(argv[2], 1);
    }
    
    if(strcmp(argv[1], "user:root") == 0)
    {
      chown(argv[2], 1);
      chgrp(argv[2], 0);
    }

    if(strcmp(argv[1], "root:root") == 0)
    {
      chown(argv[2], 0);
      chgrp(argv[2], 0);
    }

    if(strcmp(argv[1], "user:user") == 0)
    {
      chown(argv[2], 1);
      chgrp(argv[2], 1);
    }
  }
  else
  {
    printf(1, "Penggunaan: chown [OWNER][:GROUP] [file]");
  }
  
  exit();
}

