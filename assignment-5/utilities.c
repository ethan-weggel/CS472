#include <sys/stat.h>
#include "utilities.h"

long get_file_size(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    return -1; 
}