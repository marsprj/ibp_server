#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "network.h"
#include "net_sock.h"
#include "log.h"
#include "dns_cache.h"
#include "string_token.h"
#include "cmd_send.h"

//*************************************************************************
//*************************************************************************

int main(int argc, char **argv)
{
  int bufsize = 1024*1024;
  char buffer[bufsize], *bstate;
  int  i, port, timeout;
  NetStream_t *ns;
  char *host, *rid;

  if (argc < 4) {
     printf("ibp_rescan host port RID [timeout]\n");
     printf("\n");
     return(0);
  }

  i = 1;
  host = argv[i]; i++;
  port = atoi(argv[i]); i++;
  rid = argv[i]; i++;

  timeout = 15;
  if (argc < i) timeout = atoi(argv[i]);

  sprintf(buffer, "1 95 %s %d\n", rid, timeout);  // IBP_INTERNAL_RESCAN command

  assert(apr_initialize() == APR_SUCCESS);

  dns_cache_init(10);

  ns = cmd_send(host, port, buffer, &bstate, timeout);
  if (ns == NULL) return(-1);
  if (bstate != NULL) free(bstate);

  //** Close the connection
  close_netstream(ns);

  apr_terminate();

  return(0);
}
