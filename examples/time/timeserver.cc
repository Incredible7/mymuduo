#include "timeserver.h"
#include <mymuduo/Logger.h>

using namespace std::placeholders;

int main()
{
    LOG_INFO("pid = %d", getpid());
    EventLoop loop;
    InetAddress listenAddr(2037);
    TimeServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}