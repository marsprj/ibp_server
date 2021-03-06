/*
Advanced Computing Center for Research and Education Proprietary License
Version 1.0 (April 2006)

Copyright (c) 2006, Advanced Computing Center for Research and Education,
 Vanderbilt University, All rights reserved.

This Work is the sole and exclusive property of the Advanced Computing Center
for Research and Education department at Vanderbilt University.  No right to
disclose or otherwise disseminate any of the information contained herein is
granted by virtue of your possession of this software except in accordance with
the terms and conditions of a separate License Agreement entered into with
Vanderbilt University.

THE AUTHOR OR COPYRIGHT HOLDERS PROVIDES THE "WORK" ON AN "AS IS" BASIS,
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, TITLE, FITNESS FOR A PARTICULAR
PURPOSE, AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Vanderbilt University
Advanced Computing Center for Research and Education
230 Appleton Place
Nashville, TN 37203
http://www.accre.vanderbilt.edu
*/

//***************************************************************
//***************************************************************

#define _log_module_index 100

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "atomic_counter.h"
#include "iniparse.h"


#ifndef _DISABLE_LOG
#include <apr_thread_mutex.h>
FILE *_log_fd = NULL;
int _log_level = 0;
long int _log_currsize = 0;
long int _log_maxsize = 100*1024*1024;
int _log_special = 0;

apr_thread_mutex_t *_log_lock = NULL;
apr_pool_t *_log_mpool = NULL;
char _log_fname[1024] = "stdout";
int _mlog_table[_mlog_size];
char *_mlog_file_table[_mlog_size];

//***************************************************************
// _log_init - Init the log routines
//***************************************************************

void _log_init()
{
  int n;

   atomic_init();
   assert(apr_pool_create(&_log_mpool, NULL) == APR_SUCCESS);
   assert(apr_thread_mutex_create(&_log_lock, APR_THREAD_MUTEX_DEFAULT, _log_mpool) == APR_SUCCESS);
   for (n=0; n<_mlog_size; n++) { _mlog_table[n]=20; _mlog_file_table[n] = "";}
}
//***************************************************************
// _open_log - Opens the log file for access
//***************************************************************

void _open_log(char *fname, int dolock) {
  if (dolock == 1) {
     if (_log_lock == NULL) _log_init();

     _lock_log();
  }

  _log_currsize = 0;

  if (fname != NULL) {    //** If NULL we'll just use the old name
     strncpy(_log_fname, fname, sizeof(_log_fname)-1);
     _log_fname[sizeof(_log_fname)-1]= '\0';
  }

  _log_special = 0;
  if (strcmp(_log_fname, "stdout") == 0) {
     _log_special = 1;
     _log_fd = stdout;
  } else if (strcmp(_log_fname, "stderr") == 0) {
     _log_special = 2;
     _log_fd = stderr;
  } else if ((_log_fd = fopen(_log_fname, "w")) == NULL) {
     fprintf(stderr, "OPEN_LOG failed! Attempted to us log file %s\n", _log_fname);
     perror("OPEN_LOG: ");
  }

  if (dolock == 1) _unlock_log();
}

void _close_log() {
  if ((strcmp(_log_fname, "stdout") != 0) && (strcmp(_log_fname, "stderr") != 0)) {
     fclose(_log_fd);
  }
}

//***************************************************************
// mlog_printf - Prints data to the log file
//***************************************************************

int mlog_printf(int suppress_header, int module_index, int level, const char *fn, const char *fname, int line, const char *fmt, ...)
{
  va_list args;
//  int err;
  int n = 0;

  if (level > _mlog_table[module_index]) return(0);
  if (level > _log_level) return(0);

  if (_log_lock == NULL) _log_init();

  _lock_log();
  if (_log_fd == NULL) {_log_fd = stdout; _log_special=1; }

  if (suppress_header == 0) n = fprintf(_log_fd, "[mi=%d tid=%d file=%s:%d fn=%s] ", module_index, atomic_thread_id, fname, line, fn);
  va_start(args, fmt);
  n += vfprintf(_log_fd, fmt, args);
  va_end(args);

  _log_currsize += n;
  if (_log_currsize > _log_maxsize) {
//     if (_log_special==0) ftruncate(fileno(_log_fd), 0L);
     if (_log_special==0) { fclose(_log_fd); _open_log(NULL, 0); }
     _log_currsize = 0;
  }
  _unlock_log();

  return(n);
}

//***************************************************************
// flush_log - Flushes the log file
//***************************************************************

void flush_log()
{
  if (_log_lock == NULL) _log_init();

  _lock_log();
  fflush(_log_fd);
  _unlock_log();
}

//***************************************************************
// mlog_load - Loads the module log information
//***************************************************************

void mlog_load(char *fname)
{
  char *group_index, *group_level;
  char *name, *value, *logname;
  int n, default_level;
  inip_file_t *fd;
  inip_group_t *g;
  inip_element_t *ele;


  if (_log_lock == NULL) _log_init();

  group_index = "log_index";
  group_level = "log_level";

  //** Open the file
  fd = inip_read(fname);
  if (fd == NULL) {
     log_printf(0, "Error loading module definitions!  fname=%s\n", fname);
     return;
  }

  default_level = inip_get_integer(fd, group_level, "default", 0);
//printf("mlog_load: inital ll=%d\n", _log_level);
  _log_level = inip_get_integer(fd, group_level, "start_level", _log_level);
//printf("mlog_load: new ll=%d\n", _log_level);
  for (n=0; n<_mlog_size; n++) _mlog_table[n] = default_level;
  logname = inip_get_string(fd, group_level, "output", "stdout");
  open_log(logname);
  _log_maxsize = inip_get_integer(fd, group_level, "size", 100*1024*1024);

  //** Load the mappings
  g = inip_find_group(fd, group_index);
  if (g == NULL) {
     log_printf(1, "Missing %s group!\n", group_index);
     inip_destroy(fd);
     return;
  }

  ele = inip_first_element(g);
  while (ele != NULL) {
     name = inip_get_element_key(ele);
     value = inip_get_element_value(ele);

     n = (value != NULL) ? atoi(value) : -1;

     if ((n>=0) && (n<_mlog_size)) {
        _mlog_file_table[n] = strdup(name);
        _mlog_table[n] = inip_get_integer(fd, group_level, name, _mlog_table[n]);
//printf("mlog_load: mi=%d key=%s val=%d\n", n, name, _mlog_table[n]);
     } else {
       log_printf(0, "Invalid index: %s=%d  should be between 0..%d!  Skipping option\n", name, n, _mlog_size);
     }

     //** Move to the next segmnet to load
     ele = inip_next_element(ele);
  }

  inip_destroy(fd);
}

#endif


