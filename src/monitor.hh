#ifndef __MONITOR_H
#define __MONITOR_H

typedef struct _monitor_item monitor_item;
typedef struct _logfile logfile;
typedef struct _watchfile watchfile;
typedef struct _runprocess runprocess;
typedef struct _runservice runservice;
typedef struct _pid_data pid_data;
typedef struct _pid_record pid_record;
typedef struct _repository repository;
typedef struct _repo_instance repo_instance;
typedef struct _repo_branch repo_branch;
typedef struct _revision revision;

#define EVENT_SIZE	sizeof(struct inotify_event)
#define IBUF_LEN	( 1024 * (EVENT_SIZE+16) )
class Monitor {
public:
	Monitor();
	~Monitor();

	tlist *items;
	SMap *items0;
	tlist *pipes;
	int inotify_fd;
	DMap *logfiles;
	DMap *watchfiles;

	ControlFile *cmdpipe;

	Pipe *mainpipe;
	char *cfgfile;
	char *verfile;
	bool mainpipe_ready;
	bool web_online;

	bool pids_need_update;

	tlist *cmdqueue;
	void *cmd_data;
	void *repo_data;

	time_t time_now;
	char *logglyKey; // system-wide
	char *logglyTag; // just for notifications

	char *git_keyfile;
	char *git_user;
	SMap *repos;

public:
	void Setup(void); // initializes git
	void Iterate(void); // check all items. more demandcycles = more available idle time on cpu
	void Runtime(void); // start all items that aren't currently running

	void setup_repo( repository* );


	void LogCrash( runprocess*, const char *howfound );
	void LogglySend( const char *_key, const char *_tag, const char *buf );
	void LogglyPuts( monitor_item*, const char *buf );
	void Lprintf( monitor_item*, const char *, ... );
	void Lprintf( runprocess*, const char *, ... );
	void QueueCommand(const char *cmd, void *data=NULL, void *data2=NULL); // for system commands e.g., "service zyx start"

	runprocess *GetProcess( monitor_item*, const char *);

	int FD_RESET( fd_set *ins, fd_set *excepts );
	bool FD_CHECK( fd_set *ins, fd_set *excepts );

	void ReportPids(void);
	void ReportStatus(void);

	void Start(void); // start all processes

	bool Start(monitor_item *); // start all tasks associated with an item
	bool start_logfile(monitor_item *, logfile *); // register a logfile with inotify
	bool start_watchfile(monitor_item *, watchfile *);
	void force_start_process(monitor_item *, runprocess *); // start a runtime process
	bool start_process(monitor_item *, runprocess *); // start a runtime process
	void start_logfile_directory( monitor_item *item, logfile *lf_item, const char * );


	void watchfile_add_scanner(monitor_item *, watchfile *);
	void logfile_add_scanner(monitor_item *, logfile *);

	void close_process_pipe(runprocess *);

	bool Checkup(monitor_item *); // check the status of a task's items
	void checkup_repo(repository *);
	void checkup_repo_branch(repo_branch *);
	bool checkup_logfile(logfile *);
	bool checkup_logfile_directory(monitor_item *, logfile *, const char *);
	bool checkup_watchfile(watchfile *);
	bool checkup_process(runprocess *);
	bool checkup_pid(runprocess *, pid_data *);
	void recalculate_pid_cpu(runprocess *, pid_data *);
	void service_status(runprocess*);
	void psgrep_process(runprocess*);

	void verify_logfiles(monitor_item *item);
	void trim_logfiles(monitor_item *item);
	bool verify_logfile(monitor_item *item, logfile *lf, long *file_count, long *kb_count);
	bool verify_logfile_directory(monitor_item *, logfile *, const char *, long *file_count, long *kb_count);

	void Restart(void); // restart all
	void Restart(monitor_item *); // restart the given task
	bool restart_logfile(logfile *);
	bool restart_watchfile(watchfile *);
	bool restart_process(runprocess *);

	bool Stop(monitor_item *);
	bool stop_process(runprocess*);
	bool stop_logfile(logfile*);
	bool stop_watchfile(watchfile*);

	void changed_watchfile(watchfile*);
	void changed_logfile(logfile*);
};


struct _monitor_item
{
	char *name;
	int state;
	time_t start_time, started_time;
	time_t last_logfile_verify;
	char *mainlog; /// aggregate
	char *logglyKey;
	char *logglyTag;
	bool paused;
	long max_logfile_size, trim_logfile_size;
	long max_logfile_count, trim_logfile_count;
	long cur_logfile_size, cur_logfile_count;
	char *log_archive_cmd;
	tlist *logfiles; // logfile
	tlist *watchfiles; // configuration files/restart trigger
	tlist *processes; // runprocess
	tlist *requires; // char* monitor_item->name
	runprocess *ctlproc;
	Monitor *m;
};

struct _repository
{
	char *owner;
	char *name;
	char *keyfile;
	char *keyuser;
	tlist *branches;
	tlist *instances;
};

struct _repo_instance
{
	repository *repo;
	char *rootdir;
	char *branch;
	char *commit;
};

struct _repo_branch
{
	repository *repo;
	char *name;
	tlist *revisions;
};

struct _revision
{
	repo_branch *branch;
	char *commit;
	char *msg;
	time_t release_at;
	time_t build_at;
	time_t launch_at;
};


struct _watchfile
{
	char *path;
	char *action;
	bool subdirs;
	tlist *match;
	tlist *watch;
	int state;
	int watchdesc;
	bool valid;
	monitor_item *monitem;

};

struct _logfile
{
	char *path;
	tlist *subdirs; // logfile*
	off_t lastsize;
	int monitor_inotify_fd; // bleh, but it's the only variable we need from there
	int watchdesc;
	int crashlines;
	bool valid;
	tlist *dirfiles;
	SMap *dfdex;
	monitor_item *monitem;
};

struct _runprocess
{
	char *name;
	char *ctlfile; // if it exists
	char *servicename; // if it exists
	char *psgrep;

	char *cwd; // working directory
	char *env; // environment variables to set

	int last_service_check;
	int last_psgrep_check;
	int last_pid_check;

	time_t last_downtime_start;
	int start_tries;
//	time_t start_again;
	time_t start_time;
	int warmup;
	bool dubious;

	bool keeprunning;
	bool isinstance;
	bool paused;
	bool noshell, use_shellpid, use_newsid;
	int runstate; // same as monitor_item
	int cmdstate; // for psgrep/service check
	char *startcmd;
	char *stopcmd;

	tlist *pids;

	char *logtofn;
	bool autostart;
	int psgrep_missing;
//	int pid_missing;

	Pipe *shellproc;

	pid_t pid; // main pid
	monitor_item *monitem;
};

struct _pid_data
{
	int pid;
	char *procname;
	long ut_10s, st_10s, ut_30s, st_30s, ut_min, st_min;
	float per_u_10s, per_s_10s, per_u_30s, per_s_30s, per_u_min, per_s_min;
	tlist *minute;
};

struct _pid_record
{
	struct timeval at;
	struct timeval utime;
	struct timeval stime;
	char pidstate; // R = Running, S = Sleeping, D = Sleeping(Disk), Z= Zombie, T = Traced, X = dead
};

void monitor_checkup_repo_cb( Curler *, Curlpage * );
void monitor_checkup_repo_branch_cb( Curler *, Curlpage * );

monitor_item *new_monitor_item( void );
repository *new_repository( void );
repo_instance *new_repo_instance( void );
repo_branch *new_repo_branch( void );
revision *new_revision( void );
logfile *new_logfile( void );
watchfile *new_watchfile( void );
runprocess *new_runprocess( void );
pid_data *new_pid_data(void);
pid_record *new_pid_record(void);
pid_record *get_pid_record( int pid );
void free_monitor_item( monitor_item *item );
void free_repository( repository *repo );
void free_repo_instance( repo_instance *inst );
void free_repo_branch( repo_branch *branch );
void free_revision( revision *rev );
void free_logfile( logfile *item );
void free_watchfile( watchfile *item );
void free_runprocess( runprocess *item );
void free_pid_data( pid_data *item );
void free_pid_record( pid_record *item );

void free_pipe( Pipe *p );
bool pid_is_online(int pid);
void tv_set( struct timeval *x );
long tv_diff( struct timeval *a, struct timeval *b );
void tv_copy( struct timeval *x, struct timeval *a );

enum {
	MON_STATE_NONE,
	MON_STATE_INIT,
	MON_STATE_RUNNING,
	MON_STATE_CRASHED,
	MON_STATE_CHANGED,
	MON_STATE_TESTING
};

enum {
	MON_CMD_NONE,
	MON_CMD_STATUS,
	MON_CMD_PSGREP,
	MON_CMD_SCANNER
};

#endif
