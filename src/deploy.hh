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

#include "sendao.h"

#include "pipe.hh"
#include "monitor.hh"


void monitorReadHandler( Pipe *p, char *buf );
void monitorIdleHandler( Pipe *p );
void monitorCmdHandler( Pipe *p );
bool monitorTest( Pipe *p );
bool monitorTestProcess( Pipe *p );

extern Monitor *mon;

#endif
