/**
 * @copyright
 * Copyright (c) 2016-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */

#include <dfk/tcp_socket.h>
#include <dfk/internal.h>
#include <ut.h>

typedef struct
{
  dfk_t dfk;
  void* srvhandle;
} echo_fixture_t;

static void echo_fixture_setup(echo_fixture_t* f)
{
  dfk_init(&f->dfk);
  char* argv[] = {
    "127.0.0.1",
    "10020",
    "echo",
  };
  f->srvhandle = ut_pyrun_async(
      DFK_STRINGIFY(UT_TOOLS) "/tcpserver.py", DFK_SIZE(argv), argv);
  EXPECT(f->srvhandle);
  EXPECT_PYRUN(DFK_STRINGIFY(UT_TOOLS) "/tcpprobe.py", DFK_SIZE(argv), argv);
}

static void echo_fixture_teardown(echo_fixture_t* f)
{
  EXPECT(ut_pyrun_join(f->srvhandle) == 0);
  dfk_free(&f->dfk);
}

// typedef struct
// {
//   dfk_t dfk;
//   void* srvhandle;
// } boor_fixture_t;
//
// static void boor_fixture_setup(boor_fixture_t* f)
// {
//   dfk_init(&f->dfk);
//   char* argv[] = {
//     "127.0.0.1",
//     "10020",
//     "boor",
//   };
//   f->srvhandle = ut_pyrun_async(
//       DFK_STRINGIFY(UT_TOOLS) "/tcpserver.py", DFK_SIZE(argv), argv);
//   EXPECT(f->srvhandle);
// }
//
// static void boor_fixture_teardown(boor_fixture_t* f)
// {
//   EXPECT(ut_pyrun_join(f->srvhandle) == 0);
//   dfk_free(&f->dfk);
// }

static void connect_disconnect(dfk_fiber_t* fiber, void* arg)
{
  dfk_t* dfk = fiber->dfk;
  int* connected = (int*) arg;
  dfk_tcp_socket_t sock;
  EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
  EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
  *connected = 1;
  EXPECT_OK(dfk_tcp_socket_close(&sock));
}

TEST_F(echo_fixture, tcp_socket, connect_disconnect)
{
  int connected = 0;
  dfk_work(&fixture->dfk, connect_disconnect, &connected, 0);
  EXPECT(connected == 1);
}

static void single_write_read(dfk_fiber_t* fiber, void* p)
{
  DFK_UNUSED(p);
  dfk_t* dfk = fiber->dfk;
  dfk_tcp_socket_t sock;
  EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
  EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
  {
    char buffer[64] = {0};
    size_t i;
    ssize_t nread;
    for (i = 0; i < sizeof(buffer) / sizeof(buffer[0]); ++i) {
      buffer[i] = (char) (i + 24) % 256;
    }
    EXPECT(dfk_tcp_socket_write(&sock, buffer, sizeof(buffer)) == sizeof(buffer));
    memset(buffer, 0, sizeof(buffer));
    nread = dfk_tcp_socket_read(&sock, buffer, sizeof(buffer));
    EXPECT(nread > 0);
    for (i = 0; i < (size_t) nread; ++i) {
      EXPECT(buffer[i] == (char) (i + 24) % 256);
    }
  }
  EXPECT_OK(dfk_tcp_socket_close(&sock));
}

TEST_F(echo_fixture, tcp_socket, single_write_read)
{
  dfk_work(&fixture->dfk, single_write_read, NULL, 0);
}


// static void multi_write_read(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   {
//     char out[10240] = {0};
//     char in[10240] = {0};
//     ssize_t toread;
//     size_t nread, i;
//     for (i = 0; i < DFK_SIZE(out); ++i) {
//       out[i] = (char) (i + 24) % 256;
//     }
//     for (i = 0; i < 5; ++i) {
//       EXPECT(dfk_tcp_socket_write(&sock, out + i * 1024, 1024) == 1024);
//     }
//     toread = 2048;
//     while (toread > 0) {
//       nread = dfk_tcp_socket_read(&sock, in + 2048 - toread, toread);
//       EXPECT(nread > 0);
//       toread -= nread;
//     }
//     EXPECT(toread == 0);
//     toread = 2048;
//     while (toread > 0) {
//       nread = dfk_tcp_socket_read(&sock, in + 4096 - toread, toread);
//       EXPECT(nread > 0);
//       toread -= nread;
//     }
//     EXPECT(toread == 0);
//     for (i = 5; i < 10; ++i) {
//       EXPECT(dfk_tcp_socket_write(&sock, out + i * 1024, 1024) == 1024);
//     }
//     toread = sizeof(in) - 4096;
//     while (toread > 0) {
//       nread = dfk_tcp_socket_read(&sock, in + sizeof(in) - toread, toread);
//       EXPECT(nread > 0);
//       toread -= nread;
//     }
//     EXPECT(toread == 0);
//     EXPECT(memcmp(in, out, sizeof(out)) == 0);
//   }
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, multi_write_read)
// {
//   EXPECT(dfk_run(&fixture->dfk, multi_write_read, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// static void single_writev_readv(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   {
//     char buffer[64] = {0};
//     dfk_iovec_t chunks[3];
//     size_t i;
//     ssize_t nread;
//     for (i = 0; i < DFK_SIZE(buffer); ++i) {
//       buffer[i] = (char) (i + 24) % 256;
//     }
//     chunks[0].data = buffer;
//     chunks[0].size = 32;
//     chunks[1].data = buffer + 32;
//     chunks[1].size = 16;
//     chunks[2].data = buffer + 48;
//     chunks[2].size = 16;
//     EXPECT(dfk_tcp_socket_writev(&sock, chunks, 3) == sizeof(buffer));
//     memset(buffer, 0, sizeof(buffer));
//     nread = dfk_tcp_socket_readv(&sock, chunks, 3);
//     EXPECT(nread > 0);
//     for (i = 0; i < (size_t) nread; ++i) {
//       EXPECT(buffer[i] == (char) (i + 24) % 256);
//     }
//   }
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, single_writev_readv)
// {
//   EXPECT(dfk_run(&fixture->dfk, single_writev_readv, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// typedef struct {
//   dfk_t* dfk;
//   const char* endpoint;
//   uint16_t port;
//   int connected;
// } ut_connector_arg_t;
//
//
// static void* ut_connector_start_stop(void* arg)
// {
//   ut_connector_arg_t* carg = (ut_connector_arg_t*) arg;
//   int sockfd, i;
//   struct sockaddr_in addr;
//   struct timespec req, rem;
//
//   memset((char *) &addr, 0, sizeof(addr));
//   addr.sin_family = AF_INET;
//   addr.sin_port = htons(carg->port);
//   inet_aton(carg->endpoint, (struct in_addr*) &addr.sin_addr.s_addr);
//   for (i = 0; i < 32; ++i) {
//     DFK_DBG(carg->dfk, "try to connect, attempt %d", i);
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     EXPECT(sockfd != -1);
//     if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == 0) {
//       DFK_DBG(carg->dfk, "connected");
//       carg->connected += 1;
//       break;
//     }
//     /* wait for 1, 4, 9, 16, 25, ... , 1024 milliseconds */
//     memset(&req, 0, sizeof(req));
//     memset(&rem, 0, sizeof(rem));
//     req.tv_nsec = i * i * 1000000;
//     DFK_DBG(carg->dfk, "connect attempt failed, retry in %d msec", i * i);
//     close(sockfd);
//     nanosleep(&req, &rem);
//   }
//   close(sockfd);
//   pthread_exit(NULL);
//   return NULL;
// }
//
//
// static void on_new_connection_close(dfk_fiber_t* fiber, dfk_tcp_socket_t* sock, void* cbarg)
// {
//   EXPECT(fiber != NULL);
//   EXPECT(sock != NULL);
//   EXPECT(cbarg != NULL);
//   EXPECT_OK(dfk_tcp_socket_close(sock));
//   EXPECT_OK(dfk_tcp_socket_close((dfk_tcp_socket_t*) cbarg));
// }
//
//
// static void ut_listen_start_stop(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT_OK(dfk_tcp_socket_listen(&sock, "127.0.0.1", 10000, on_new_connection_close, &sock, 0));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST(tcp_socket, listen_start_stop)
// {
//   pthread_t cthread;
//   ut_connector_arg_t carg;
//   dfk_t dfk;
//
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_listen_start_stop, NULL, 0));
//
//   carg.dfk = &dfk;
//   carg.port = 10000;
//   carg.endpoint = "127.0.0.1";
//   carg.connected = 0;
//   EXPECT(!pthread_create(&cthread, NULL, &ut_connector_start_stop, &carg));
//
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
//
//   EXPECT(pthread_join(cthread, NULL) == 0);
//   EXPECT(carg.connected == 1);
// }
//
//
// static void* ut_connector_read_write(void* arg)
// {
//   ut_connector_arg_t* carg = (ut_connector_arg_t*) arg;
//   int sockfd;
//   unsigned int i;
//   ssize_t nread = 0, nwritten = 0;
//   size_t totalread = 0;
//   struct sockaddr_in addr;
//   struct timespec req, rem;
//   unsigned char buf[1234] = {0};
//
//   memset((char *) &addr, 0, sizeof(addr));
//   addr.sin_family = AF_INET;
//   addr.sin_port = htons(carg->port);
//   inet_aton(carg->endpoint, (struct in_addr*) &addr.sin_addr.s_addr);
//   for (i = 0; i < 32; ++i) {
//     DFK_DBG(carg->dfk, "try to connect, attempt %d", i);
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     EXPECT(sockfd != -1);
//     if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == 0) {
//       DFK_DBG(carg->dfk, "connected");
//       carg->connected += 1;
//       break;
//     }
//     /* wait for 1, 4, 9, 16, 25, ... , 1024 milliseconds */
//     memset(&req, 0, sizeof(req));
//     memset(&rem, 0, sizeof(rem));
//     req.tv_nsec = i * i * 1000000;
//     DFK_DBG(carg->dfk, "connect attempt failed, retry in %d msec", i * i);
//     close(sockfd);
//     nanosleep(&req, &rem);
//   }
//   for (i = 0; i < sizeof(buf); ++i) {
//     buf[i] = i % 256;
//   }
//   nwritten = write(sockfd, buf, sizeof(buf));
//   EXPECT(nwritten == sizeof(buf));
//   memset(buf, 0, sizeof(buf));
//   do {
//     nread = read(sockfd, buf + totalread, sizeof(buf) - totalread);
//     EXPECT(nread > 0);
//     totalread += nread;
//   } while (totalread < sizeof(buf));
//   for (i = 0; i < sizeof(buf); ++i) {
//     EXPECT(buf[i] == i % 256);
//   }
//   close(sockfd);
//   return NULL;
// }
//
//
// static void on_new_connection_echo(dfk_fiber_t* fiber, dfk_tcp_socket_t* sock, void* cbarg)
// {
//   EXPECT(fiber != NULL);
//   EXPECT(sock != NULL);
//   EXPECT(cbarg != NULL);
//
//   DFK_UNUSED(cbarg);
//
//   for (;;) {
//     char buf[512] = {0};
//     ssize_t nbytes = dfk_tcp_socket_read(sock, buf, sizeof(buf));
//     if (nbytes < 0) {
//       break;
//     }
//     nbytes = dfk_tcp_socket_write(sock, buf, nbytes);
//     if (nbytes < 0) {
//       break;
//     }
//   }
//
//   EXPECT_OK(dfk_tcp_socket_close(sock));
//   EXPECT_OK(dfk_tcp_socket_close((dfk_tcp_socket_t*) cbarg));
// }
//
//
// static void ut_listen_read_write(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT_OK(dfk_tcp_socket_listen(&sock, "127.0.0.1", 10000, on_new_connection_echo, &sock, 10));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST(tcp_socket, listen_read_write)
// {
//   pthread_t cthread;
//   ut_connector_arg_t carg;
//   dfk_t dfk;
//
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_listen_read_write, NULL, 0));
//
//   carg.dfk = &dfk;
//   carg.port = 10000;
//   carg.endpoint = "127.0.0.1";
//   carg.connected = 0;
//   EXPECT(!pthread_create(&cthread, NULL, &ut_connector_read_write, &carg));
//
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
//
//   EXPECT(pthread_join(cthread, NULL) == 0);
//   EXPECT(carg.connected == 1);
// }
//
//
// static void ut_connect_failed(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT(dfk_tcp_socket_connect(&sock, "127.0.0.1", 12345) == dfk_err_sys);
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST(tcp_socket, connect_failed)
// {
//   dfk_t dfk;
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_connect_failed, NULL, 0));
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
// }
//
//
// static void ut_read_failed(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   char buf[1024] = {0};
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   EXPECT(dfk_tcp_socket_read(&sock, buf, sizeof(buf)) < 0);
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(boor_fixture, tcp_socket, read_failed)
// {
//   dfk_t dfk;
//   DFK_UNUSED(fixture);
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_read_failed, NULL, 0));
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
// }
//
//
// static void ut_write_failed(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   char buf[1024] = {0};
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   {
//     ssize_t nwritten = 1;
//     while (nwritten > 0) {
//       nwritten = dfk_tcp_socket_write(&sock, buf, sizeof(buf));
//     }
//   }
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(boor_fixture, tcp_socket, write_failed)
// {
//   dfk_t dfk;
//   DFK_UNUSED(fixture);
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_write_failed, NULL, 0));
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
// }
//
//
// static void ut_read_not_connected(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   char buf[1024] = {0};
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT(dfk_tcp_socket_read(&sock, buf, sizeof(buf)) == dfk_err_badarg);
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST(tcp_socket, read_not_connected)
// {
//   dfk_t dfk;
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_read_not_connected, NULL, 0));
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
// }
//
//
// static void ut_write_not_connected(dfk_fiber_t* fiber, void* p)
// {
//   dfk_tcp_socket_t sock;
//   char buf[1024] = {0};
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, fiber->dfk));
//   EXPECT(dfk_tcp_socket_write(&sock, buf, sizeof(buf)) == dfk_err_badarg);
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST(tcp_socket, write_not_connected)
// {
//   dfk_t dfk;
//   EXPECT_OK(dfk_init(&dfk));
//   EXPECT(dfk_run(&dfk, ut_write_not_connected, NULL, 0));
//   EXPECT_OK(dfk_work(&dfk));
//   EXPECT_OK(dfk_free(&dfk));
// }
//
//
// static void ut_connect_already_connected(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   EXPECT(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020) == dfk_err_badarg);
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, connect_already_connected)
// {
//   EXPECT(dfk_run(&fixture->dfk, ut_connect_already_connected, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// static void ut_listen_connected(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   EXPECT(dfk_tcp_socket_listen(&sock, "127.0.0.1", 10000, on_new_connection_close, NULL, 0) == dfk_err_badarg);
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, listen_connected)
// {
//   EXPECT(dfk_run(&fixture->dfk, ut_listen_connected, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// static void ut_double_close(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, double_close)
// {
//   EXPECT(dfk_run(&fixture->dfk, ut_double_close, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// static void ut_zero_read_write(dfk_fiber_t* fiber, void* p)
// {
//   dfk_t* dfk = fiber->dfk;
//   dfk_tcp_socket_t sock;
//   char buf[3] = {0};
//   dfk_iovec_t iov;
//   DFK_UNUSED(p);
//   EXPECT_OK(dfk_tcp_socket_init(&sock, dfk));
//   EXPECT_OK(dfk_tcp_socket_connect(&sock, "127.0.0.1", 10020));
//   EXPECT_OK(dfk_tcp_socket_write(&sock, buf, 0));
//   EXPECT_OK(dfk_tcp_socket_writev(&sock, &iov, 0));
//   EXPECT_OK(dfk_tcp_socket_read(&sock, buf, 0));
//   EXPECT_OK(dfk_tcp_socket_readv(&sock, &iov, 0));
//   EXPECT_OK(dfk_tcp_socket_close(&sock));
//   EXPECT_OK(dfk_tcp_socket_free(&sock));
// }
//
//
// TEST_F(echo_fixture, tcp_socket, zero_read_write)
// {
//   EXPECT(dfk_run(&fixture->dfk, ut_zero_read_write, NULL, 0));
//   EXPECT_OK(dfk_work(&fixture->dfk));
// }
//
//
// TEST(tcp_socket, errors)
// {
//   dfk_t dfk;
//   dfk_tcp_socket_t sock;
//   char buf[1];
//   dfk_iovec_t iov[1];
//
//   EXPECT(dfk_tcp_socket_init(NULL, NULL) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_init(&sock, NULL) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_init(NULL, &dfk) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_free(NULL) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_connect(NULL, "127.0.0.1", 10000) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_connect(&sock, NULL, 10000) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_listen(NULL, "127.0.0.1", 10000, on_new_connection_close, NULL, 0) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_listen(&sock, NULL, 10000, on_new_connection_close, NULL, 0) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_listen(&sock, "127.0.0.1", 10000, NULL, NULL, 0) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_close(NULL) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_read(NULL, buf, sizeof(buf)) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_read(&sock, NULL, sizeof(buf)) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_readv(NULL, iov, DFK_SIZE(iov)) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_readv(&sock, NULL, DFK_SIZE(iov)) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_write(NULL, buf, sizeof(buf)) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_write(&sock, NULL, sizeof(buf)) == dfk_err_badarg);
//
//   EXPECT(dfk_tcp_socket_writev(NULL, iov, DFK_SIZE(iov)) == dfk_err_badarg);
//   EXPECT(dfk_tcp_socket_writev(&sock, NULL, DFK_SIZE(iov)) == dfk_err_badarg);
// }

