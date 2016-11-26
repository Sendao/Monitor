#ifndef __PIPE_HH
#define __PIPE_HH

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stropts.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "sendao.h"


typedef class Pipe Pipe;

typedef bool pipe_cb( Pipe* );
typedef void pipe_read_cb( Pipe*, char* );
typedef bool pipe_readtest_cb( Pipe*, char* );
typedef bool pipe_test_cb( Pipe* );


class Pipe {
	public:
	char *logfn;
	int fdm;
	int childproc;
	char **logbuf;
	void *data;

	char *cwd;
	char *usecwd, *useenv;
	char *username;
	char *cmdprompt;
	bool detect_new_cwd, detect_new_username, repeat_new_cwd;
	bool support_password_block, prompt_reset;
	bool matched_cmdline, use_newsid;

	stringbuf *readbuffer;

	pipe_read_cb *readCB; // read input
	pipe_cb *idleCB; // on select() read timeout
	pipe_cb *cmdlineCB; // on prompt

	pipe_readtest_cb *readBreakTest; // for breaking out of inputloop()
	pipe_test_cb *breakTest;

	bool write_to_stdout, read_from_stdin;
	public:
	Pipe();
	~Pipe();
	void cleanstring(char *);// remove terminal codes
	bool open(void);
	void reopen(void); // restart the shell
	void chdir( const char *newcwd );
	void setenv( const char *newenv );
	char *match_commandline(char *inbuf);
	void getcommandprompt(void);
	void FD_TSET(fd_set *ins, fd_set *excepts);
	int FD_TEST(fd_set *ins, fd_set *excepts);
	int FD_READ(char *buf, int len);
	void log(const char *logfilename);	// adds a logging facility
	void logtobuf(char **);
	void run(char* const args[]);
	void exec(char**,const char *);
	void iterableloop(pipe_test_cb *);
	void inputloop(long max_useconds=0);
	int waitchild(int);
	int read(char*,int,bool nonblocking=false);
	int write(const char *);
	void close(void);
};

bool pipe_test_newline( Pipe *p, char *buf );

#endif
