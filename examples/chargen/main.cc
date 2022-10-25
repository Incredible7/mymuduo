#include "chargen.h"
#include <mymuduo/Logger.h>
#include <unistd.h>

int main()
{
    LOG_INFO("pid = %d", getpid());
    EventLoop loop;
    InetAddress listenAddr(2015);
    ChargenServer server(&loop, listenAddr, true);
    server.start();
    loop.loop();
}
