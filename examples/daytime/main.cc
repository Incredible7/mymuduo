#include "daytime.h"

#include <mymuduo/Logger.h>

#include <unistd.h>

int main()
{
    LOG_INFO("pid = %d", getpid());
    EventLoop loop;
    InetAddress listenAddr(2022);
    DaytimeServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}
