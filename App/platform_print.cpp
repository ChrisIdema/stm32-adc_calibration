#include  <sys/unistd.h>

#ifdef DEBUG

#include "main.h" // for ITM_SendChar

extern "C" int _write(int file, char* data, int size)
{
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
    {
        return -1;
    }

    for(int i=0;i<size;++i)
    {
        ITM_SendChar(data[i]);
    }

    return size;
}
#else
//print in release mode
#endif
