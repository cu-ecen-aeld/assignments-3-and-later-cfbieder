#include <syslog.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[]){

  //Open log file
  openlog(NULL, 0, LOG_USER);

  //Check of all arguments, if not error
  if (argc != 3){
    syslog(LOG_ERR, "Error: %d Parameters required, actual parameters required 2)", argc-1);
    return 1;
  }

  //Write File
  char* writefile = argv[1];
  char* writestr = argv[2];

  //Open write file
  FILE *filePointer;
  filePointer = fopen(writefile , "w");

  //Check for Error   if (fp == NULL)
  if (filePointer == NULL)
  {
     syslog(LOG_ERR, "Error: Unable to open file.\n");
     return 1;
  }

  //Write file and close
  int status = fputs(writestr, filePointer);
  if (status != EOF) {
    syslog(LOG_DEBUG, "Writing %s to %s", writestr , writefile);
  }
  else {
    syslog(LOG_ERR, "Error: Unable to write file.\n");
  }

  fclose(filePointer);


  return 0;

}
