#ifndef __DEPLOY_HH
#define __DEPLOY_HH

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef _USE_CURL
#include <curl.h>
#endif

#include "sendao.h"

#include "pipe.hh"
#include "control.hh"
#include "curler.hh"
#include "monitor.hh"

void monitorReadHandler( Pipe *p, char *buf );
bool monitorIdleHandler( Pipe *p );
bool monitorCmdHandler( Pipe *p );
bool monitorTest( Pipe *p );
bool monitorTestProcess( Pipe *p );

void mainLoop(void);

void mainReadHandler( Pipe *p, char *buf );
bool mainIdleHandler( Pipe *p );
bool mainCmdHandler( Pipe *p );


bool pipeTest( Pipe *p );

extern Monitor *mon;
extern monitor_item *cfg_item;
extern logfile *cfg_lf;
extern runprocess *cfg_rp;
extern Curler *curl;
extern bool is_daemon;

void loadConfig( const char *path );
long DiskSize( const char *size );
void lprintf( const char *, ... );
void dlog( int flag, const char *, ... );

#define LOG_ALL	2047 // est.
#define LOG_NONE 0
#define LOG_ONE	1
#define LOG_TWO	2
#define LOG_THREE 4
#define LOG_FOUR 8
#define LOG_FIVE 16
#define LOG_SIX 32
#define LOG_SEVEN 64
#define LOG_EIGHT 128
#define LOG_NINE 256
#define LOG_TEN 512
#define LOG_ZERO 1024

#endif
