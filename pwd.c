#include "types.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "user.h"

#define NOLL   ((void*)0)
#define WRONG  (0)
#define TRUTH  (1)

#define PATH_SEPARATOR   "/"

// @param fd   file descriptor for a directory.
// @param ino  target inode number.
// @param p    [out] file name (part of absPath), overwritten by the file name of the ino.
static int dirlookup(int fd, int ino, char* p) {
  struct dirent de;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    if (de.inum == ino) {
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = '\0';
      return TRUTH;
    }
  }
  return WRONG;
}

static char* goUp(int ino, char* ancestorPath, char* resultPath) {
  strcpy(ancestorPath + strlen(ancestorPath), PATH_SEPARATOR "..");
  struct stat st;
  if (stat(ancestorPath, &st) < 0)
    return NOLL;

  if (st.ino == ino) {
    // No parent directory exists: must be the root.
    return resultPath;
  }

  char* foundPath = NOLL;
  int fd = open(ancestorPath, O_RDONLY);
  if (fd >= 0) {
    char* p = goUp(st.ino, ancestorPath, resultPath);
    if (p != NOLL) {
      strcpy(p, PATH_SEPARATOR);
      p += sizeof(PATH_SEPARATOR) - 1;

      // Find current directory.
      if (dirlookup(fd, ino, p))
        foundPath = p + strlen(p);
    }
    close(fd);
  }
  return foundPath;
}

static int getcwd(char* resultPath) 
{

  	resultPath[0] = '\0';

  	char ancestorPath[512];
  	strcpy(ancestorPath, ".");

  	struct stat st;
  	if (stat(ancestorPath, &st) < 0)
    return WRONG;

  	char* p = goUp(st.ino, ancestorPath, resultPath);
  	if (p == NOLL)
    	return WRONG;
  	if (resultPath[0] == '\0')
    	strcpy(resultPath, PATH_SEPARATOR);
  	return TRUTH;
}

int main(int argc, char *argv[]) {
  char resultPath[512];
  if (getcwd(resultPath))
    printf(1, "%s\n", resultPath);
  else
    printf(2, "Pwd gagal!");
  exit();
}