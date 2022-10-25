#include "discard.h"
#include <mymuduo/Logger.h>
#include <unistd.h>

int main()
{
    LOG_INFO("pid = %d", getpid());
    EventLoop loop;
    InetAddress listenAddr(2021);
    DiscardServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}
