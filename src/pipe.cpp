#include "main.hh"


Pipe::Pipe(void)
{
	fdm = -1;
	childproc = -1;
	logfn = NULL;
	logbuf = NULL;
	readbuffer = new stringbuf(1024);
	read_from_stdin = write_to_stdout = true;
	support_password_block = false;
	detect_new_username = detect_new_cwd = false;
	repeat_new_cwd = false;
	prompt_reset = false;
	readCB = NULL;
	idleCB = NULL;
	cmdlineCB = NULL;
	readBreakTest = NULL;
	breakTest = NULL;
	username = NULL;
	cmdprompt = NULL;
	cwd = NULL;
	data = NULL;
	useenv = NULL;
	usecwd = NULL;
	matched_cmdline = false;
	use_newsid = true;
}

Pipe::~Pipe()
{
	if( fdm > 0 ) this->close();

	if( logfn ) free(logfn);
	if( username ) free(username);
	if( cmdprompt ) free(cmdprompt);
	if( readbuffer ) delete readbuffer;
}

void Pipe::log(const char *logfilename)
{
	logfn = strdup( logfilename );
}

void Pipe::logtobuf( char **lbuf )
{
	logbuf = lbuf;
	if( logbuf )
		*logbuf = NULL;
}

bool pipe_test_newline( Pipe *p, char *buf )
{
	return ( strstr(buf,"\n") != NULL );
}

bool Pipe::open(void)
{
	char **args;

	readbuffer->clear();

	args = (char**) calloc( sizeof(char*), 3 );
	args[0] = strdup("bash");
	args[1] = strdup("-i");
	args[2] = NULL;

	run( args );
	free( args[0] );
	free( args[1] );
	free( args );

	return true;
}

void Pipe::exec(char **lbuf, const char *cmd)
{
	char *args[256];
	int i;
	const char *p;
	char buf[1025], *pbuf;
	
	logbuf = lbuf;
	*lbuf = NULL;
	i=0;
	for( p = cmd, pbuf=buf; *p; p++ ) {
		if( *p == ' ' ) {
			if( buf != pbuf ) {
				*pbuf = '\0';
				args[i++] = strdup( buf );
				pbuf = buf;
				continue;
			}
		} else {
			*pbuf++ = *p;
		}
	}
	if( buf != pbuf ) {
		*pbuf = '\0';
		args[i++] = strdup( buf );
	}
	args[i] = NULL;
	
	run( args );

	while( i > 0 ) {
		--i;
		free(args[i]);
	}
}


void Pipe::reopen(void)
{
	// close then reopen the pipe.
	if( fdm != -1 ) {
		write("exit\n");
		close();
	}

	char **args;

	args = (char**) calloc( sizeof(char*), 3 );
	args[0] = strdup("bash");
	args[1] = strdup("-i");
	args[2] = NULL;

	run( args );
	free( args[0] );
	free( args[1] );
	free( args );
}

void Pipe::run(char* const args[])
{
//	int i;

	close();
	// create psuedoterminal pair
	fdm = ::open("/dev/ptmx", O_RDWR);
	if( fdm < 0 ) {
		perror("open");
		abort();
	}
	
	grantpt( fdm );
	unlockpt( fdm );
	char *slavename = ptsname(fdm);

	lprintf("fork(%s)", use_newsid ? "new_session" : "same_session");
	// Fork into a shell
	childproc = fork();
	if( childproc == 0 ) {
		// enter psuedoterminal slave
		if( fdm >= 0 )
			::close(fdm);
		
		if( use_newsid )
			setsid();

		if( usecwd ) { // since we're not actually in a shell, we just chdir
			::chdir( usecwd );
		}
		if( useenv ) {
			tlist *envs = split(useenv, "\n");
			tnode *n;
			char *sV;
			tlist *vars;
			forTLIST( sV, n, envs, char* ) {
				vars = split( sV, "=" );
				if( vars->count == 2 ) {
					::setenv( (char*)vars->FindData(0), (char*)vars->FindData(1), 1 );
				} else {
					lprintf("Error: Wrong varscount: %s", sV);
				}
				vars->Clear(free);
				delete vars;
			}
			envs->Clear(free);
			delete envs;
		}
		
		int fds = ::open(slavename, O_RDWR);
		if( fds < 0 ) {
			perror("open");
			abort();
		}
		
		//ioctl( fds, I_PUSH, "ptem" );
		//ioctl( fds, I_PUSH, "ldterm" );
		//ioctl( fds, I_PUSH, "ptem" );
		//ioctl( fds, I_PUSH, "ldterm" );

		// Redirect into 'fds'
		if( dup2( fds, STDIN_FILENO ) != STDIN_FILENO ) {
			lprintf("dup2 stdin failed");
			abort();
		}
		if( dup2( fds, STDOUT_FILENO ) != STDOUT_FILENO ) {
			lprintf("dup2 stdout failed");
			abort();
		}
		if( dup2( fds, STDERR_FILENO ) != STDERR_FILENO ) {
			lprintf("dup2 stderr failed");
			abort();
		}
		// Grab the client shell
		
		struct termios tio;
		if (tcgetattr(0, &tio) < 0) {
			perror( "tcgetattr" );
			abort();
		}
		//tio.c_oflag |= OCRNL;
		tio.c_oflag &= ~OPOST;
		tio.c_lflag |= ICANON | ISIG;
		tio.c_lflag &= ~ECHO;
		//tio.c_cflag &= ~CMSPAR;

		/*for( i = 0; i < NCCS; i++ ) {
			tio.c_cc[i] = 0;
		}
		*/
		if (tcsetattr(0, TCSANOW, &tio) < 0) {
			perror( "tcsetattr" );
			abort();
		}
		
		/*
		fprintf( stdout, "pipe " );
		for( i = 0; args[i]; i++ ) {
			fprintf( stdout, "%s ", args[i]);
		}
		fprintf( "" );
		*/
		
		lprintf("child:run(%s)", args[0]);
		//execlp( "echo", "hello", "world");
		execvp( args[0], args );
		//execlp("/usr/local/bin/bash", "bash", "-l", "-i", NULL);
		
		perror("execvp");
		abort();
	}
		
	struct termios tio;
	if (tcgetattr(fdm, &tio) < 0) {
		perror( "tcgetattr" );
		abort();
	}
	tio.c_oflag &= ~OPOST;
//	tio.c_oflag |= ONLCR;
	tio.c_iflag |= IGNCR; 
	tio.c_lflag &= ~ECHO;
	if (tcsetattr(fdm, TCSANOW, &tio) < 0) {
		perror( "tcsetattr" );
		abort();
	}
}

void Pipe::cleanstring( char *buf )
{/* leave the ANSI in now that terminal support is decent
	char *pBuf = buf, *pTgt = buf;

	while( *pBuf ) {
		while( *pBuf && ( *pBuf == '\r' || ( !isspace(*pBuf) && !isprint(*pBuf) ) ) ) {
			pBuf++;
		}
		if( pTgt != pBuf )
			*pTgt = *pBuf;
		pTgt++;
		pBuf++;
	}
	*pTgt = '\0'; */
}

int Pipe::read(char *buf, int maxlen, bool nonblocking)
{
	int n, maxfd=fdm+1;
	fd_set ins;
	char *tptr;
	char *bufprompt;
	size_t size;

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO( &ins );
	FD_SET( fdm, &ins );
	select( maxfd, &ins, NULL, NULL, &timeout );
	
	if( FD_ISSET( fdm, &ins ) ) {
		n = ::read( fdm, buf, maxlen-1 );
		if( n <= 0 ) {
			buf[0] = '\0';
			return n;
		}
		
		buf[n] = '\0';
		//lprintf("Read: '%s'", buf);
		cleanstring(buf);
		//lprintf("read[%d]:%s", n, buf);
		if( support_password_block && ( (bufprompt=strstr(buf, "Password:")) != NULL ) ) {
			if( readbuffer->len > 32000 ) {
				readbuffer->clear();
			}
			readbuffer->append(buf);
			if( readCB )
				readCB(this, buf );

			lprintf("Enter password: ");
			size = 128;
			tptr = (char*)malloc(size);
			while( getline(&tptr, &size, stdin) == -1 ) {
				lprintf("Try again. Enter password: ");
			}
			write(tptr);
			free(tptr);
			support_password_block = false;
			prompt_reset = true;
		} else {
			if( cmdprompt && (bufprompt=match_commandline(buf))!=NULL ) { // trim output after a commandline appears
				*bufprompt = '\0';
				matched_cmdline=true;
			}
			if( readbuffer->len > 32000 ) {
				readbuffer->clear();
			}
			readbuffer->append(buf);
			if( readCB )
				readCB(this, buf );
		}

		if( logfn ) {
			FILE *fp;
			fp = fopen( logfn, "a" );
			if( fp ) {
				fputs( buf, fp );
				fclose( fp );
			} else {
				lprintf("Cannot write to logfile '%s': %s", logfn, buf );
			}
		}
		if( logbuf ) {
			if( *logbuf ) {
				tptr = (char*)realloc( *logbuf, strlen( *logbuf ) + n + 5 );
				strcat(tptr, buf);
			} else {
				tptr = (char*)malloc( n + 5 );
				strcpy(tptr, buf);
			}
			*logbuf = tptr;
		}
		return n;
	}
	
	return 0;
}

int Pipe::write(const char *buf)
{
	char buf2[1025];
	int len;

	char *tmpx = cmdprompt;
	cmdprompt=NULL;
	len = read( buf2, 1024 ); // this->read: read any remaining input
	if( len > 0 ) {
		lprintf("note: read on write: '%s'", buf2);
		if( write_to_stdout )
			::write( STDOUT_FILENO, buf, len ); // write to the real stdout
	}
	cmdprompt=tmpx;

	len = strlen(buf);

	if( logfn ) {
		FILE *fp;
		fp = fopen( logfn, "a" );
		if( fp ) {
			fputs( buf, fp );
			fclose( fp );
		} else {
			fprintf(stderr, "Unable to append file %s", logfn);
		}
	}
	
	readbuffer->clear(); // readbuffer->clear: clear remnant input to prepare for next line
	return ::write( fdm, buf, len );
}

int Pipe::waitchild(int flags)
{
	int n;
	
	if( childproc == -1 )
		return 1;
	
	n = waitpid( childproc, NULL, flags );
	if( n > 0 )
		childproc = -1;
	
	return n;
}
void Pipe::chdir( const char *newcwd )
{
	if( usecwd ) free(usecwd);
	usecwd = strdup(newcwd);
}
void Pipe::setenv( const char *newenv )
{
	if( useenv ) free(useenv);
	useenv = strdup(newenv);
}

char *Pipe::match_commandline( char *inbuf )
{
	uint32_t len = strlen(inbuf), plen = strlen(cmdprompt), cwdlen = strlen(this->cwd), userlen = strlen(this->username);
	char *pInbuf, *pCmdline, *pCwd;

	pInbuf = inbuf + len;
	pCmdline = cmdprompt + plen-1;

	while( *pInbuf == '\0' || isspace(*pInbuf) ) pInbuf--;

	do {
		if( *pCmdline == ',' ) {
			if( detect_new_cwd ) {
				pCwd=pInbuf+1;
				pCmdline--;
				while( *pInbuf != *pCmdline ) {
					pInbuf--;
				}
				if( this->cwd ) free(this->cwd);
				this->cwd = strndup( pInbuf+1, (int)(pCwd-pInbuf)-1 );
				lprintf("Detected new cwd: '%s'", this->cwd);
				if( !repeat_new_cwd )
					detect_new_cwd = false;
			} else {
				if( ( pInbuf+1-cwdlen < inbuf ) || ( str_cn_cmp( pInbuf+1-cwdlen, this->cwd ) != 0 ) ) {
					return NULL;
				}
				pInbuf -= cwdlen;
				pCmdline--;
			}
		} else if( *pCmdline == '$' && *pInbuf == '#' ) {
			pInbuf--;
			pCmdline--;
		} else if( *pCmdline == '`' ) {
			if( detect_new_username ) {
				pCwd=pInbuf+1;
				pCmdline--;
				while( pInbuf > inbuf && *pInbuf != *pCmdline ) {
					pInbuf--;
				}
				if( this->username ) free(this->username);
				this->username = strndup( pInbuf+1, (int)(pCwd-pInbuf)-1 );
				lprintf("Detected new username: '%s'", this->username);
				detect_new_username = false;
			} else {
				if( ( pInbuf+1-userlen < inbuf ) || ( str_cn_cmp( pInbuf+1-userlen, this->username ) != 0 ) ) {
					return NULL;
				}
				pInbuf -= userlen;
				pCmdline--;
			}
		} else if( *pCmdline != *pInbuf ) {
			return NULL;
		} else {
			pInbuf--;
			pCmdline--;
		}
	} while( pInbuf > inbuf && pCmdline > cmdprompt );

	return ( pCmdline <= cmdprompt ) ? pInbuf : NULL;
}

void Pipe::getcommandprompt( void )
{
	//lprintf("show prompt");

	// temporarily disable normal hooks
	pipe_read_cb *tempRead = readCB;
	pipe_cb *tempIdle = idleCB;

	pipe_readtest_cb *tempReadTest = readBreakTest;
	pipe_test_cb *tempTest = breakTest;
	readCB = NULL;
	idleCB = NULL;
	breakTest = NULL;
	readBreakTest = pipe_test_newline;
	char buf[512];
//	char *tmp_cmdline;

	stringbuf *sb;

	readbuffer->clear();

	// trigger a commandline prompt to be displayed
	lprintf("\ngetcommandprompt()");
	if( this->cmdprompt ) {
		free(this->cmdprompt);
		this->cmdprompt = NULL;
	}
	write("echo start_tag && PS1=_\\\\u:\\\\w# && whoami && pwd\n");

	// run loops to fill the input buffer
	int x;
	unsigned i=0;
//	int attempts=0;
	int n=0;
	char *searchptr, *sptr2;

	while( n < 3 ) {
		inputloop(2000000); // read one line, or two seconds

		while( n < 3 ) {
			//lprintf("Read buffer %d: '%s'", n, readbuffer->p+i);

			if( i > readbuffer->len ) {
				i = 0;
			}
			if( readbuffer->p )
				searchptr = strstr(readbuffer->p+i, "\n");
			else searchptr = NULL;

			if( searchptr ) {
				x = (searchptr - readbuffer->p);
			} else if( n == 0 ) {
				// need new input?
				write("echo start_tag && PS1=_\\\\u:\\\\w# && whoami && pwd\n");
				i=x=0;
				break;
			} else {
				//readbuffer->clear();
				break;
			}

			if( n == 0 ) {
			// verify start_tag
				sptr2 = strstr( readbuffer->p+i, "start_tag" );
				if( sptr2 && sptr2 < searchptr ) {
					n=1;
					i = x+1;
				} else {
					readbuffer->clear();
				}
			} else if( n == 1 ) {

				sptr2 = strstr( readbuffer->p+i, " " );
				if( !sptr2 || sptr2 > searchptr ) {
					n=2;

			// find username
					if( this->username ) free(this->username);
					this->username = strndup( readbuffer->p+i, x-i );
					lprintf("Working username: '%s'", this->username);
					i = x+1;
				}

			} else if( n == 2 ) {
				n=3;

			// find cwd
				x = searchptr - readbuffer->p;
				if( this->cwd ) free(this->cwd);
				this->cwd = strndup( readbuffer->p+i, x-i );

				sb = new stringbuf(this->cwd);
				if( str_c_cmp(this->username,"root") == 0 ) {
					sprintf(buf, "/%s", this->username);
				} else {
					sprintf(buf, "/home/%s", this->username);
				}
				sb->replace( buf, "~" );
				free( this->cwd );
				this->cwd = strdup(sb->p);
				delete sb;

				lprintf("Working directory: '%s'", this->cwd);
				i = x+1;

			} else if( n == 3 ) {
				/* we force the prompt instead of reading it :/

				x = (searchptr - readbuffer->p);
				tmp_cmdline = strndup( readbuffer->p+i, x );
				lprintf("Command source: '%s'", tmp_cmdline);
				strip_newline(tmp_cmdline);
				i = x+1;

				sb = new stringbuf(tmp_cmdline);
				sb->replace( this->cwd, "," );
				sb->replace( this->username, "`" );
				this->cmdprompt = strdup(sb->p);
				delete sb;
				lprintf("Command prompt: '%s'", this->cmdprompt);
				*/

				n = 4;
				break;
			}
		}
	}

	cmdprompt = strdup("_`:,$");
//	lprintf(" Prompt looks like: '%s'", cmdprompt?cmdprompt:"null");
	//print_string(cmdprompt);

	// re-enable hooks
	readCB = tempRead;
	idleCB = tempIdle;
	breakTest = tempTest;
	readBreakTest = tempReadTest;

	// normal processing resumes
}
void Pipe::inputloop(long tv_seconds)
{
	fd_set ins, outs, excepts;
	int maxfd;
	char buf[1025];
	int len;
	struct timeval timeout;
	bool stdin_unavailable = false;
	
//	lprintf("parent:inputloop");
	maxfd = fdm+1;
		
	while( 1 ) {
		FD_ZERO( &ins );
		FD_ZERO( &outs );
		FD_ZERO( &excepts );
		
		if( read_from_stdin ) {
			FD_SET( STDIN_FILENO, &ins ); // messages from keyboard/user
			FD_SET( STDIN_FILENO, &excepts );
		}
		FD_SET( fdm, &ins ); // messages from pipe
		FD_SET( fdm, &excepts );
		
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000;
		if( tv_seconds > 0 ) {
			tv_seconds -= 200000;
			if( tv_seconds < 0 )
				return;
		}
		select( maxfd, &ins, NULL, &excepts, &timeout );

		/* Read from stdin and write to pipe */
		if( !stdin_unavailable && FD_ISSET( STDIN_FILENO, &ins ) ) {
			len = ::read( STDIN_FILENO, buf, 1024 );
			if( len == 0 ) {
				stdin_unavailable = true;
				lprintf( "EOF read on stdin. Closing it.");
			} else {
				buf[len] = '\0';
				::write( fdm, buf, len );
				// fsync( fdm );
			}
		}
		
		/* Read from pipe, echo to stdout */
		if( FD_ISSET( fdm, &ins ) ) {
			len = read( buf, 1024 ); // this->read
			if( len > 0 ) {
				if( write_to_stdout )
					::write( STDOUT_FILENO, buf, len ); // write to the real stdout

				if( readBreakTest && readBreakTest(this, buf) ) {
					return;
				}
			} else {
				lprintf( "EOF on pipe." );
				break;
			}
		} else {
			if( prompt_reset ) {
				prompt_reset=false;
				getcommandprompt();
				write("echo\n");
			} else if( matched_cmdline ) {
				matched_cmdline = false;
				if( cmdlineCB )
					cmdlineCB(this);
			} else if( idleCB ) {
				idleCB(this);
			}
		}
		
		if( FD_ISSET(fdm, &excepts) ) {
			lprintf( "Session ended: pipe crashed." );
			break;
		}
		if( FD_ISSET(0, &excepts) ) {
			lprintf( "Session ended: channel crashed." );
			break;
		}
		
		/*
		if( waitchild(WNOHANG) != 0 ) {
			lprintf( "Session ended peacefully." );
			break;
		}
		*/

		if( breakTest && breakTest(this) ) {
			::write( fdm, "exit\n", 5 );
			break;
		}
	}
	
	maxfd++;
	lprintf( "Waiting for child to exit");
	waitchild( WNOHANG );
	lprintf( "Closing pipe" );
	// disconnect
	close();
}

void Pipe::FD_TSET(fd_set *ins, fd_set *excepts)
{
	if( fdm < 0 ) return; //!

	FD_SET( fdm, ins ); // messages from pipe
	FD_SET( fdm, excepts );
}
int Pipe::FD_TEST(fd_set *ins, fd_set *excepts)
{
	if( fdm < 0 ) return -1; //!

	if( FD_ISSET( fdm, excepts ) ) {
		lprintf( "Session ended: pipe crashed." );
		close();
		return -1;
	} else if( FD_ISSET( fdm, ins ) ) {
		return 1;
	}
	return 0;
}
int Pipe::FD_READ(char *buf, int bufsize)
{
	int len = read( buf, bufsize );
	if( len <= 0 ) {
		lprintf( "EOF on pipe." );
		return len;
	}
	if( write_to_stdout )
		::write( STDOUT_FILENO, buf, len ); // write to the real stdout

	if( readBreakTest && readBreakTest(this, buf) ) {
		return 0;
	} else {
		if( prompt_reset ) {
			prompt_reset=false;
			getcommandprompt();
			write("echo\n");
		} else if( matched_cmdline ) {
			matched_cmdline = false;
			if( cmdlineCB && !cmdlineCB(this) ) {
				return -1;
			}
		} else if( idleCB ) {
			idleCB(this);
		}
	}

	if( breakTest && breakTest(this) ) {
		::write( fdm, "exit\n", 5 );
		return 0;
	}

	return len;
}
void Pipe::close(void)
{
	if( fdm != -1 ) {
		write("exit\n");
		::close( fdm );
		fdm = -1;
	}
	if( childproc > 0 ) {
		waitchild(WNOHANG);
		childproc = -1;
	}
}
