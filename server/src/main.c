#include "../inc/netcomm.h"
#include "../inc/packet.h"
#include "../inc/server.h"

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (1 < argc)
    {
        port = atoi(argv[1]);
    }

    server_setup(port);
    return 0;
}