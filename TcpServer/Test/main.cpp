#include "tcp_server.hpp"

int main()
{
    TcpServer tcp_server;

    if (!tcp_server.initialize())
    {
        return 1;
    }

    tcp_server.run();

    if (!tcp_server.shutdown())
    {
        return 1;
    }

    return 0;
}

