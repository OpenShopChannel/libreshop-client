#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

int delete_directory(char* directory_path) {
    DIR* dir = opendir(directory_path);
    if (dir == NULL) return 1;

    struct dirent *entry;
    struct stat statbuf;
    
    int isdir;
    int ret;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        int fullnamelen = strlen(directory_path) + strlen(entry->d_name) + 2; // plus nul and an extra byte if nul needs to be moved for a recursive call
        char* fullname = malloc(fullnamelen);

        sprintf(fullname, "%s%s", directory_path, entry->d_name);

        stat(fullname, &statbuf);
        isdir = S_ISDIR(statbuf.st_mode);
        
        if (isdir) {
            fullname[fullnamelen - 2] = '/';
            fullname[fullnamelen - 1] = '\0';

            ret = delete_directory(fullname);
            if (ret != 0) return ret;
        }
        else {
            ret = unlink(fullname);
            if (ret != 0) return ret;
        }
    }

    closedir(dir);

    ret = rmdir(directory_path);
    return ret;
}
