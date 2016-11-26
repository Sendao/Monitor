#ifndef __MONITOR_CONTROL_H
#define __MONITOR_CONTROL_H

typedef class ControlFile ControlFile;
typedef struct _ControlStatement ControlStatement;
typedef void control_cb(tlist *args);

class ControlFile
{
public:
	ControlFile(const char *mainpath);
	~ControlFile();

	int inotify_fd, inotify_wd;
	char *cmdfn;
	SMap *handlers;

public:
	int FD_RESET( fd_set *ins, fd_set *exs );
	void FD_CHECK( fd_set *ins, fd_set *exs );
	void checkup(void);
	void checkup(const char *fn);
	void commands(const char *filebuf);
	void RegisterCommand( const char *cmd, control_cb *handler );
};



#endif
