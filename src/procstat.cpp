#include "main.hh"

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
