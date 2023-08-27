#include <stdio.h>
#include <sandia.h>

void sandia_ext_write_to_file(sandia* s, FILE* fp) {
    int buflen = 256;
    char buf[buflen];
    s32 received;
    while ((received = net_recv(s->_sandia_socket._fd, buf, buflen, 0)) > 0) {
        fwrite(buf, 1, received, fp);
    }
}
