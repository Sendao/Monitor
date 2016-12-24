#include "main.hh"

// main logger
int logflags = LOG_ONE | LOG_TWO;// | LOG_THREE | LOG_FOUR;

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

    fp = fopen(mainlogfn, "a");
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

    fp = fopen(mainlogfn, "a");
	if( !fp ) {
		return;
	}
	fprintf(fp, "%d:%d:%d %ld.%06ld %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, sb.p);
    fclose(fp);
}
void Monitor::Lprintf( monitor_item *item, const char *fmt, ... )
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

    if( item->mainlog )
    	fp = fopen(item->mainlog, "a");
    else if( mainlogfn )
    	fp = fopen(mainlogfn, "a");
    else
    	fp = NULL;
    fprintf(stdout, "%d:%d:%d %ld.%06ld %s %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, item->name, sb.p);
    fflush(stdout);

    if( logglyKey && logglyTag ) {
    	Lprintf( item, "%ld.%06ld %s %s", tv_now.tv_sec, tv_now.tv_usec, item->name, sb.p);
    }
	if( !fp ) {
		return;
	}
	fprintf(fp, "%d:%d:%d %ld.%06ld %s %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, item->name, sb.p);
    fclose(fp);
}

void Monitor::Lprintf( runprocess *rp, const char *fmt, ... )
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

    fp = fopen(rp->logtofn, "a");
	fprintf(stdout, "%d:%d:%d %ld.%06ld %s %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, rp->name, sb.p);
	fflush(stdout);
    if( logglyKey && logglyTag ) {
    	Lprintf( rp->monitem, "%ld.%06ld %s %s", tv_now.tv_sec, tv_now.tv_usec, rp->name, sb.p);
    }
	if( !fp ) {
		return;
	}
	fprintf(fp, "%d:%d:%d %ld.%06ld %s %s\n", realtime.tm_hour, realtime.tm_min, realtime.tm_sec, tv_now.tv_sec, tv_now.tv_usec, rp->name, sb.p);
    fclose(fp);
}

void Monitor::LogglySend( const char *_key, const char *_tag, const char *buf )
{
	stringbuf sbfix, sa;
	sa.clear();
	sa.printf("http://logs-01.loggly.com/inputs/%s/tag/%s/", _key ? _key : logglyKey, _tag ? _tag : logglyTag);

	// send to the logfile

	Curlpage *cp = curl->Get(sa.p, NULL);
	cp->attachdata(buf);
	cp->open();
	/*
	//sa.printf("curl -s -H \"content-type:text/plain\" -d \"%s\" "http://logs-01.loggly.com/inputs/%s/tag/%s/" >> /var/log/curl.monitor.log", sbfix.p, _key ? _key : logglyKey, _tag ? _tag : logglyTag);
	if( fork() == 0 ) {
		execl("/bin/sh", "sh", "-c", sa.p, (char*)0);
	}
	*/
}

void Monitor::LogCrash( runprocess *rp, const char *howfound )
{
	tnode *n, *n2;
	monitor_item *item;
	runprocess *rp2;
	bool found=false;
	stringbuf *sa;

	dlog(LOG_TWO, "crash(%p,%s)", rp, howfound);

	if( rp->isinstance ) {
		//! We need some way to distinguish graceful closures, but for now, instances will not be logged.
		return;
	}

	lprintf("CrashLog[%s]: Restarting Process.", howfound);
	stop_process(rp);

	forTLIST( item, n, items, monitor_item* ) {
		forTLIST( rp2, n2, item->processes, runprocess* ) {
			if( rp2 == rp ) {
				found=true;
				break;
			}
		}
		if( found )
			break;
	}
	if( !found ) {
		lprintf("Couldn't find process group to crash dlogile.");
		return;
	}

	logfile *lf;
	int crashnumber = number_range(10000,99999);

	sa = new stringbuf();
	sa->printf("rm -f '%s.crash%d'\n", item->name, crashnumber);
	QueueCommand(sa->p);

	sa->clear();
	sa->printf("echo \"%s\" > '%s.crash%d'", howfound, item->name, crashnumber);

	forTLIST( lf, n, item->logfiles, logfile* ) {
		if( sa->len > 0 ) sa->append(" ; ");
		sa->printf("tail -n%d %s >> '%s.crash%d'", lf->crashlines, lf->path, item->name, crashnumber);
	}
	sa->append("\n");
	lprintf("LogCmd: %s", sa->p);
	QueueCommand(sa->p);

	if( ( logglyKey || item->logglyKey ) && ( logglyTag || item->logglyTag ) ) {
        struct timeval tv_now;
        gettimeofday( &tv_now, NULL );
        tv_now.tv_sec %= 1000;

    	Lprintf( item, "%ld.%06ld %s %s", tv_now.tv_sec, tv_now.tv_usec, item->name, howfound);
    }

	sa->clear();
	sa->printf("./send-slack.sh '%s.crash%d' '#ithinkitcrashed' 'as-bot'\n", item->name, crashnumber);
	lprintf("LogCmd: %s", sa->p);
	QueueCommand(sa->p);
	delete sa;

	start_process(item, rp);
}




