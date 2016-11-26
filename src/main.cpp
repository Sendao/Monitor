#include "main.hh"

Monitor *mon;
Curler *curl;
char *my_hostname;
bool is_daemon=false;

int main(int ac, char *av[])
{
	if( ac >= 2 && str_cn_cmp(av[1], "daemon") == 0 ) { // start as a daemon
		is_daemon=true;
		lprintf("Spawning as daemon.");
		daemon(1,0); // daemonize, keep directory, close stdin.
	} else {
		lprintf("Spawning as runtime process.");
	}
	mon = new Monitor();
	curl = new Curler();

	if( ac < 2 ) {
		loadConfig("/etc/monitor.cfg");
	} else {
		lprintf("Using config file '%s'...", av[ac-1]);
		loadConfig(av[ac-1]);
	}
	//! get hostname
//	char hnbuf[1024];
//	gethostname(hnbuf, 1024);
//	hostname = strdup(hnbuf);

	mon->Start(); // start the processes. todo: dependencies
	// do runtime loop
	mainLoop();

	lprintf("exit()");
	delete mon;
	delete curl;

	return 0;
}

void mainLoop( void )
{
	fd_set ins, outs, excepts;
	int maxfd;
	char buf[1025];
	int len;
	struct timeval timeout, tv_now, tv_read;
	bool stdin_unavailable = is_daemon;
	int idle_cps = 7200;
	int activity_cps = 400;
	int web_cps = 200;
	int cur_cps = 100;

	long c_next = 100, c_idle=10;
	int cycles = 0;

	gettimeofday( &tv_read, NULL );
	while( 1 ) {

		if( c_next > 10000 ) c_next=10000; // max sleep of 10 seconds
		timeout.tv_sec = 0;
		timeout.tv_usec = c_next*1000; // sleep until next cycle starts
		tv_set(&timeout);
		//lprintf("timeout: %ld..%ld", timeout.tv_sec, timeout.tv_usec);

		FD_ZERO( &ins );
		FD_ZERO( &outs );
		FD_ZERO( &excepts );

		if( !stdin_unavailable ) {
			FD_SET( STDIN_FILENO, &ins ); // messages from keyboard/user
			FD_SET( STDIN_FILENO, &excepts );
		}

		maxfd = mon->FD_RESET( &ins, &excepts ) + 1;
		dlog( LOG_SIX, "Enter idle" );

		select( maxfd, &ins, NULL, &excepts, &timeout ); // wake up if any events occur
		gettimeofday( &tv_now, NULL );

		dlog( LOG_SIX, "Exit idle" );


		c_idle = tv_diff( &tv_now, &tv_read ); // how long did we work+sleep
		tv_copy( &tv_read, &tv_now );

		//printf("\r[cps:%d next:%ld idle:%ld]", cur_cps, c_next, c_idle);

		if( c_idle > c_next/2 ) { // input was slow or nonexistant or workload was high
			if( !mon->web_online && cur_cps < idle_cps ) { // slow down to cruising speed
				cur_cps *= 8;
			}
		}
		if( c_idle < c_next/4 ) { // input was fairly fast compared to low workload
			if( cur_cps > activity_cps ) { // speed up
				cur_cps /= 2;
			}
		}


		c_next -= c_idle; // c_next points at start of next cycle

		cycles=0; // skip some cycles if we idled past
		if( cur_cps <= 0 ) cur_cps = 100;
		while( c_next <= 0 ) {
			cycles++;
			c_next += cur_cps;
		}
		if( cycles > 0 ) {
			dlog(LOG_ONE, "cycles %ld c_next %ld cps %ld timer %ld %ld", cycles, c_next, cur_cps, c_next, c_idle);
		} else {
			dlog(LOG_EIGHT, "cycles %ld c_next %ld cps %ld timer %ld %ld", cycles, c_next, cur_cps, c_next, c_idle);
		}
		/* Read from stdin, maybe run commands */
		if( !stdin_unavailable && FD_ISSET( STDIN_FILENO, &ins ) ) {
			len = ::read( STDIN_FILENO, buf, 1024 );
			if( len == 0 ) {
				lprintf( "EOF read on stdin. Closing it.");
				stdin_unavailable = true;
			} else {
				buf[len] = '\0';
				//::write( fdm, buf, len );
				// fsync( fdm );
				strip_newline(buf);
				lprintf("Received command: '%s'. Ignoring it.", buf);
			}
		}

		// Check pipes
		if( mon->FD_CHECK( &ins, &excepts ) || mon->web_online  ) { // there is activity
			if( ( !mon->web_online && cur_cps < activity_cps ) || ( mon->web_online && cur_cps < web_cps ) ) {
				cur_cps /= 2; // speed up scanning to match
			}
		}

		if( cycles > 0 ) {
			dlog( LOG_ONE, "Work" );
//			lprintf("cycles:%d cps:%d", cycles, cur_cps);
			mon->Iterate();

			if( mon->web_online && cur_cps > web_cps ) {
				cur_cps = web_cps;
				c_next = cur_cps;
			}
		}

/*		if( breakTest && breakTest(this) ) {
			::write( fdm, "exit\n", 5 );
			break;
		} */
	}
}

