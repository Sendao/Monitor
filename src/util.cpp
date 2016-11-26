#include "main.hh"

// main logger
int logflags = LOG_ONE | LOG_TWO | LOG_THREE | LOG_FOUR;

void dlog( int flag, const char *fmt, ... )
{
	if( ( logflags & flag ) != flag ) return;

	stringbuf sb;
    FILE *fp;
    va_list args;
    struct timeval tv_now;

    va_start(args, fmt);
    sb.vsprintf(fmt, args);
    va_end(args);

    gettimeofday( &tv_now, NULL );
    tv_now.tv_sec %= 1000;

    struct tm realtime;
    time_t tmx;
    tmx = time(NULL);
    localtime_r( &tmx, &realtime );

    fp = fopen("/tmp/monitor.log", "a");
	if( !fp ) {
		return;
	}
	fprintf(fp, "%d:%d:%d %ld.%06ld %d:%s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, flag, sb.p);
    fclose(fp);
}
void lprintf( const char *fmt, ... )
{
	stringbuf sb;
    FILE *fp;
    va_list args;
    struct timeval tv_now;

    va_start(args, fmt);
    sb.vsprintf(fmt, args);
    va_end(args);

    gettimeofday( &tv_now, NULL );
    tv_now.tv_sec %= 1000;

    struct tm realtime;
    time_t tmx;
    tmx = time(NULL);
    localtime_r( &tmx, &realtime );

    fp = fopen("/tmp/monitor.log", "a");
	if( !fp ) {
		return;
	}
	fprintf(fp, "%d:%d:%d %ld.%06ld %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, sb.p);
    fclose(fp);
}
// process management
bool pid_is_online( int pid )
{
	char procbuf[128];
	sprintf(procbuf, "/proc/%d/statm", pid);
	char *process_stats = readwholefile(procbuf);

	if( !process_stats ) {
		// process died
		return false;
	}
	free(process_stats);
	return true;
}

// time management
void tv_set( struct timeval *x )
{
	while( x->tv_usec > 1000000 ) {
		x->tv_usec -= 1000000;
		++x->tv_sec;
	}
	while( x->tv_usec < 0 ) {
		--x->tv_sec;
		x->tv_usec+=1000000;
	}
}

long tv_diff( struct timeval *a, struct timeval *b )
{
	return (long)( a->tv_sec - b->tv_sec ) * 1000 + ( a->tv_usec - b->tv_usec ) / 1000;
}

void tv_copy( struct timeval *x, struct timeval *a )
{
	x->tv_sec = a->tv_sec;
	x->tv_usec = a->tv_usec;
}

long DiskSize( const char *value )
{
	const char *suffix, *p;

	for( p=value; *p && isdigit(*p); p++ );
	if( !*p ) {
		return atol(value);
	}
	char *intbuf = strndup( value, p-value );
	suffix = p;
	if( str_cn_cmp(suffix, "kb") == 0 ) {
		return atol(intbuf);
	} else if( str_cn_cmp(suffix, "mb") == 0 ) { // matches 'mb', 'M', 'm'
		return atol(intbuf) * 1024;
	} else if( str_cn_cmp(suffix, "gb") == 0 ) {
		return atol(intbuf) * 1024000;
	} else if( str_cn_cmp(suffix, "tb") == 0 ) {
		return atol(intbuf) * 1000000000;
	}
	return atol(intbuf);
}




// memory management

monitor_item *new_monitor_item( void )
{
	monitor_item *item = (monitor_item*)malloc(sizeof(monitor_item));
	memset( item, 0, sizeof(*item) );
	item->logfiles = new tlist();
	item->watchfiles = new tlist();
	item->processes = new tlist();
	item->requires = new tlist();
	return item;
}
watchfile *new_watchfile( void )
{
	watchfile *item = (watchfile*)malloc(sizeof(watchfile));
	memset( item, 0, sizeof(*item) );
	item->watch = new tlist();
	item->match = new tlist();
	return item;
}
repository *new_repository( void )
{
	repository *v = (repository*)malloc(sizeof(repository));
	memset( v, 0, sizeof(*v) );
	v->branches = new tlist;
	v->instances = new tlist;
	return v;
}
repo_instance *new_repo_instance( void )
{
	repo_instance *v = (repo_instance*)malloc(sizeof(repo_instance));
	memset( v, 0, sizeof(*v) );
	return v;
}
repo_branch *new_repo_branch( void )
{
	repo_branch *v = (repo_branch*)malloc(sizeof(repo_branch));
	memset( v, 0, sizeof(*v) );
	v->revisions = new tlist;
	return v;
}
revision *new_revision( void )
{
	revision *v = (revision*)malloc(sizeof(revision));
	memset( v, 0, sizeof(*v) );
	return v;
}
logfile *new_logfile( void )
{
	logfile *item = (logfile*)malloc(sizeof(*item));
	memset( item, 0, sizeof(*item) );
	item->subdirs = new tlist();
	item->crashlines = 8;
	return item;
}
runprocess *new_runprocess( void )
{
	runprocess *item = (runprocess*)malloc(sizeof(*item));
	memset( item, 0, sizeof(*item) );
	item->pids = new tlist();
	item->use_shellpid = true;
	item->keeprunning = true;
	item->autostart = true;
	item->use_newsid = true;
	item->last_downtime_start = time(NULL);
	return item;
}

pid_data *new_pid_data( void )
{
	pid_data *item = (pid_data*)malloc(sizeof(*item));
	memset( item, 0, sizeof(*item) );
	item->minute = new tlist();
	return item;
}

pid_record *new_pid_record( void )
{
	pid_record *item = (pid_record*)malloc(sizeof(*item));
	item->at.tv_usec = item->at.tv_sec = 0;
	item->utime.tv_usec = item->utime.tv_sec = 0;
	item->stime.tv_usec = item->stime.tv_sec = 0;
	memset( item, 0, sizeof(*item) );
	return item;
}

pid_record *get_pid_record( int pid )
{
	char buf[256];
	sprintf(buf, "/proc/%d/stat", pid);
	char *filebuf = readFile(buf);
	if( !filebuf || !*filebuf ) return NULL;

	tlist *details = split(filebuf, " ");
	pid_record *pr = new_pid_record();
	gettimeofday( &pr->at, NULL );
	char *arg;
	arg = (char*)details->FindData(2);
	if( arg )
		pr->pidstate = *arg;
	long ut = atol( (char*)details->FindData(15) );
	long st = atol( (char*)details->FindData(16) );

	pr->utime.tv_usec = ut;
	tv_set(&pr->utime);
	pr->stime.tv_usec = st;
	tv_set(&pr->stime);
	return pr;
}

void free_monitor_item( monitor_item *item )
{
	if( item->logfiles ) {
		item->logfiles->Clear((voidFunction*)free_logfile);
		delete item->logfiles;
	}
	if( item->watchfiles ) {
		item->watchfiles->Clear((voidFunction*)free_watchfile);
		delete item->watchfiles;
	}
	if( item->processes ) {
		item->processes->Clear((voidFunction*)free_runprocess);
		delete item->processes;
	}
	if( item->requires ) {
		item->requires->Clear((voidFunction*)free);
		delete item->requires;
	}

	if( item->ctlproc )
		free_runprocess(item->ctlproc);

	if( item->mainlog )
		free(item->mainlog);

	if( item->logglyKey )
		free(item->logglyKey);

	if( item->logglyTag )
		free(item->logglyTag);

	if( item->log_archive_cmd )
		free(item->log_archive_cmd);

	if( item->name )
		free(item->name);
	free(item);
}

void free_watchfile( watchfile *item )
{
	item->match->Clear( free );
	delete item->match;
	item->match = NULL;

	item->watch->Clear( free );
	delete item->watch;
	item->watch = NULL;

	item->valid = false;

	free(item);
}

void free_logfile( logfile *item )
{
	if( item->watchdesc >= 0 ) {
		inotify_rm_watch( item->monitor_inotify_fd, item->watchdesc );
	}
	if( item->path )
		free(item->path);
	if( item->subdirs ) {
		item->subdirs->Clear( (voidFunction*)free_logfile );
		delete item->subdirs;
		item->subdirs = NULL;
	}
	item->valid = false;
	if( item->dirfiles ) {
		item->dirfiles->Clear( (voidFunction*)free_logfile );
		delete item->dirfiles;
	}
	if( item->dfdex ) delete item->dfdex;
	free(item);
}

void free_runprocess( runprocess *item )
{
	if( item->ctlfile )
		free(item->ctlfile);
	if( item->servicename )
		free(item->servicename);
	if( item->startcmd )
		free(item->startcmd);
	if( item->stopcmd )
		free(item->stopcmd);
	if( item->logtofn )
		free(item->logtofn);
	if( item->psgrep )
		free(item->psgrep);
	if( item->pids ) {
		item->pids->Clear( (voidFunction*)free_pid_data );
		delete item->pids;
	}
	if( item->name )
		free(item->name);
	if( item->cwd )
		free(item->cwd);
	if( item->env )
		free(item->env);

	free(item);
}
void free_pid_data( pid_data *item )
{
	if( item->minute ) {
		item->minute->Clear( (voidFunction*)free_pid_record );
		delete item->minute;
	}
	if( item->procname )
		free(item->procname);

	free(item);
}
void free_pid_record( pid_record *item )
{
	free(item);
}



void free_pipe( Pipe *p )
{
	delete p;
}

void free_repository( repository *repo )
{
	if( repo->owner )
		free( repo->owner );
	if( repo->name )
		free( repo->name );
	if( repo->keyfile )
		free( repo->keyfile );
	if( repo->keyuser )
		free( repo->keyuser );
	repo->branches->Clear( (voidFunction*)free_repo_branch );
	delete repo->branches;
	repo->instances->Clear( (voidFunction*)free_repo_instance );
	delete repo->instances;
	free(repo);
}
void free_repo_instance( repo_instance *inst )
{
	if( inst->rootdir )
		free(inst->rootdir);
	if( inst->branch )
		free(inst->branch);
	if( inst->commit )
		free(inst->commit);
	free(inst);
}
void free_repo_branch( repo_branch *branch )
{
	branch->revisions->Clear( (voidFunction*)free_revision );
	delete branch->revisions;
	free(branch);
}
void free_revision( revision *rev )
{
	free(rev);
}
