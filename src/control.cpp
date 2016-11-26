#include "main.hh"


ControlFile::ControlFile( const char *mainpath )
{
	cmdfn = strdup(mainpath);
	handlers = new SMap(16);
	inotify_fd = inotify_init1(IN_NONBLOCK);

	if( inotify_fd < 0 ) {
		lprintf("control inotify is not working.");
		lprintf("control inotify is not working.");
		lprintf("control inotify is not working.");
	}

	inotify_wd = inotify_add_watch( inotify_fd, cmdfn, IN_CLOSE_WRITE );
}
ControlFile::~ControlFile()
{
	inotify_rm_watch( inotify_fd, inotify_wd );
	if( cmdfn ) free(cmdfn);
	close(inotify_fd);
	delete handlers;
}

int ControlFile::FD_RESET( fd_set *ins, fd_set *exs )
{
	int maxfd=0;

	FD_SET( inotify_fd, ins );
	maxfd = inotify_fd+1;

	return maxfd;
}

void ControlFile::FD_CHECK( fd_set *ins, fd_set *exs )
{
	if( !FD_ISSET( inotify_fd, ins ) )
		return;

	lprintf("ControlFile::update");

	char ibuf[IBUF_LEN];
	struct inotify_event *ev;
	ssize_t i, len;

	for(;;) {

		len = read( inotify_fd, ibuf, IBUF_LEN );

		if( len <= 0 ) {
			if( errno == EAGAIN )
				break;
			lprintf("EOF on controlfile.");
		} else {
			lprintf("Got Control events: %ld", len);
			i=0;
			do {
				ev = (struct inotify_event*)&ibuf[i];

				i += EVENT_SIZE + ev->len;
				// if( ev->wd == inotify_wd ) ...
				if( ( ev->mask & IN_DELETE ) == IN_DELETE ) continue;

				if( ev->len ) {
					lprintf("%s", ev->name);
					checkup( ev->name );
				}

				/*
				forTLIST( item, n, items, monitor_item* ) {
					forTLIST( lf, nn, item->logfiles, logfile* ) {

					}
				}*/
			} while( i < len );
			//checkup();
		}
	}

}

void ControlFile::RegisterCommand( const char *cmd, control_cb *h)
{
	handlers->Set(cmd, (void*)h);
}

void ControlFile::checkup(void)
{
	if( !check_file_exists(cmdfn) ) return;

	char *filebuf = readFile(cmdfn);
	unlink(cmdfn);

	commands(filebuf);

	free(filebuf);
}

void ControlFile::checkup( const char *fn )
{

	char *filebuf, *filename;

	filename = (char*)malloc(strlen(fn) + 3 + strlen(cmdfn));
	sprintf(filename, "%s/%s", cmdfn, fn);
	if( !check_file_exists(filename) ) {
		lprintf("Bad command file %s/%s", cmdfn, fn);
		free(filename);
		return;
	}
	filebuf = readFile(filename);
	lprintf("Read %s from %s", filebuf, filename);

	unlink(filename);

	commands(filebuf);
	free(filename);
	free(filebuf);
}

void ControlFile::commands(const char *filebuf)
{
	tlist *cmds = split(filebuf, "\n");

	tnode *n;
	char *cmdLine;
	tlist *cmdbuf;
	char *cmd;
	control_cb *h;

	forTLIST( cmdLine, n, cmds, char* ) {
		cmdbuf = split(cmdLine, ":");
		if( ! cmdbuf ) continue;
		if( cmdbuf->count > 0 ) {
			cmd = (char*)cmdbuf->FullPop();
			if( *cmd ) {
				h = (control_cb*)handlers->Get(cmd);
				if( h ) {
					h(cmdbuf);
				} else {
					fprintf(stdout, "Unknown command '%s' line '%s'\n", cmd, cmdLine);
				}
			}
			cmdbuf->Clear( free );
		}
		delete cmdbuf;
	}

	cmds->Clear( free );
	delete cmds;
}



