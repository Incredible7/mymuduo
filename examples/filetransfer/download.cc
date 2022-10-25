#include <mymuduo/Logger.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/TcpServer.h>

#include <stdio.h>
#include <unistd.h>

const char *g_file = NULL;

// FIXME: use FileUtil::readFile()
std::string readFile(const char *filename)
{
  std::string content;
  FILE *fp = ::fopen(filename, "rb");
  if (fp)
  {
    // inefficient!!!
    const int kBufSize = 1024 * 1024; // 1M
    char iobuf[kBufSize];
    ::setbuffer(fp, iobuf, sizeof iobuf);

    char buf[kBufSize];
    size_t nread = 0;
    while ((nread = ::fread(buf, 1, sizeof buf, fp)) > 0)
    {
      content.append(buf, nread);
    }
    ::fclose(fp);
  }
  return content;
}

void onHighWaterMark(const TcpConnectionPtr &conn, size_t len)
{
  LOG_INFO("HighWaterMark %d", len);
}

void onConnection(const TcpConnectionPtr &conn)
{
  std::string state;
  if (conn->connected())
  {
    state = "UP";
  }
  else
  {
    state = "DOWN";
  }
  LOG_INFO("TimeServer - %s -> %s is %s",
           conn->peerAddress().toIpPort().c_str(),
           conn->localAddress().toIpPort().c_str(), state.c_str());
  if (conn->connected())
  {
    LOG_INFO("FileServer - Sending file %s to %s", g_file,
             conn->peerAddress().toIpPort().c_str());
    conn->setHighWaterMarkCallback(onHighWaterMark, 64 * 1024);
    std::string fileContent = readFile(g_file);
    conn->send(fileContent);
    conn->shutdown();
    LOG_INFO("FileServer - done");
  }
}

int main(int argc, char *argv[])
{
  LOG_INFO("pid = %d", getpid());
  if (argc > 1)
  {
    g_file = argv[1];

    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

// g++ -o FileServer1 download.cc -pthread -lmymuduo -std=c++11 -g