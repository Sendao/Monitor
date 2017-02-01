#include "main.hh"
#include <dirent.h>

void cmd_start(tlist *args)
{
	char *groupname = (char*)args->FullPop();
	monitor_item *item = (monitor_item*)mon->items0->Get( groupname );
	if( !item ) {
		lprintf("Start: Cannot find group '%s'", groupname);
		return;
	}
	char *procname = (char*)args->FullPop();
	if( procname ) {
		runprocess *rp = mon->GetProcess(item, procname);
		if( rp ) {
			mon->force_start_process(item, rp);
			return;
		}
	}
	item->paused = false;
	mon->Start(item);
}

void cmd_restart(tlist *args)
{
	char *groupname = (char*)args->FullPop();
	monitor_item *item = (monitor_item*)mon->items0->Get( groupname );
	if( !item ) {
		lprintf("Restart: Cannot find group '%s'", groupname);
		return;
	}
	char *procname = (char*)args->FullPop();
	if( procname ) {
		runprocess *rp = mon->GetProcess(item, procname);
		if( rp ) {
			rp->paused = false;
			mon->restart_process(rp);
			return;
		}

	}
	item->paused = false;
	mon->Restart(item);
}

void cmd_stop(tlist *args)
{
	char *groupname = (char*)args->FullPop();
	monitor_item *item = (monitor_item*)mon->items0->Get( groupname );
	if( !item ) {
		lprintf("Stop: Cannot find group '%s'", groupname);
		return;
	}
	char *procname = (char*)args->FullPop();
	if( procname ) {
		runprocess *rp = mon->GetProcess(item, procname);
		if( rp ) {
			rp->paused = true;
			mon->stop_process(rp);
			return;
		}

	}
	item->paused = true;
	mon->Stop(item);
}

void cmd_instance(tlist *args)
{
	char *groupname = (char*)args->FullPop();
	char *itemname = (char*)args->FullPop();
	char *itempid = (char*)args->FullPop();
	int pid;

	if( itempid ) pid = atoi(itempid);
	if( !groupname || !itemname || !itempid || pid == 0 ) {
		lprintf("Instance: syntax instance <groupname> <itemname> <pid>");
		return;
	}
	monitor_item *item = (monitor_item*)mon->items0->Get( groupname );
	if( !item ) {
		lprintf("Instance: Cannot find group '%s'", groupname);
		return;
	}

	runprocess *rp = new_runprocess();
	rp->name = strdup(itemname);
	rp->pid = (pid_t)pid;
	rp->psgrep = strdup(itempid);
	rp->autostart = false;
	rp->keeprunning = false;
	rp->isinstance = true;
	rp->cmdstate = MON_CMD_NONE;
	rp->runstate = MON_STATE_RUNNING;
	item->processes->PushBack(rp);
	mon->Checkup(item);

}

Monitor::Monitor()
{
	items = new tlist;
	items0 = new SMap(64);

	pipes = new tlist;
	logfiles = new DMap(16);
	watchfiles = new DMap(16);
	pids_need_update = false;

	inotify_fd = inotify_init();

	if( inotify_fd < 0 ) {
		lprintf("inotify is not working.");
		lprintf("inotify is not working.");
		lprintf("inotify is not working.");
	}

	cmdqueue = new tlist();

	cmdpipe = new ControlFile("/tmp/monitor.cmd");
	cmdpipe->RegisterCommand( "start", cmd_start );
	cmdpipe->RegisterCommand( "stop", cmd_stop );
	cmdpipe->RegisterCommand( "restart", cmd_restart );
	cmdpipe->RegisterCommand( "instance", cmd_instance );

	cfgfile = NULL;

	mainpipe = new Pipe();

	mainpipe->log("/tmp/monitor.log");
	mainpipe->readCB = mainReadHandler;
//	mainpipe->idleCB = mainIdleHandler;
	mainpipe->cmdlineCB = mainCmdHandler;

	mainpipe->write_to_stdout = false;
	mainpipe->read_from_stdin = false;

	mainpipe->data = (void*)this;
	//mainpipe->use_new_sid = true;

	mainpipe->open();
	mainpipe->getcommandprompt();

	mainpipe_ready = true;
	web_online = false;
	cmd_data = NULL;

	repos = new SMap(32);

	logglyKey = NULL;
	logglyTag = strdup("monitor");

	return;
}
Monitor::~Monitor()
{
	delete cmdpipe;

	pipes->Clear( (voidFunction*)free_pipe );
	items->Clear( (voidFunction*)free_monitor_item );
	delete items;
	delete items0;
	delete pipes;
	if( logglyKey ) free(logglyKey);
	if( logglyTag ) free(logglyTag);
	if( cfgfile ) free(cfgfile);
	if( inotify_fd >= 0 ) {
		close( inotify_fd );
	}
	delete logfiles;
	delete watchfiles;
}


void Monitor::Setup( void )
{
	tlist *repoList = repos->ToList();
	tnode *n;
	repository *r;

	forTLIST( r, n, repoList, repository* ) {
		setup_repo(r);
		checkup_repo(r);
	}
}
void Monitor::setup_repo( repository *repo )
{
	//! load cached data if available
}
void Monitor::checkup_repo( repository *repo )
{
	stringbuf sa;
	sa.printf("GIT_SSH='ssh -i %s \\$@' git ls-remote --heads https://github.com/%s/%s", repo->owner, repo->name);
	// curl -H "Authorization: token ace9154957568968f0064de3dbf75c3b1b51f0be" https://api.github.com/repos/ooblahman/alpha-sheets/branches
	// curl -i -u 'sendao@gmail.com' -d '{"scopes":["repo","user","public_repo"],"note":"silver1311"}' https://api.github.com/authorizations
	// +password
	//QueueCommand( sa.p,
	//Curlpage *cp = curl->Get(sa.p, monitor_checkup_repo_cb);
	//cp->userdata = (void*)repo;
	//cp->open();
}

void monitor_checkup_repo_cb( Curler *c, Curlpage *cp )
{
	stringbuf sa;
	repository *repo = (repository*)cp->userdata;
	repo_branch *branch;
	tnode *n;

	//! verify git permissions
	//! process
	printf("Repo branch list:\n%s", cp->contents->p);

	forTLIST( branch, n, repo->branches, repo_branch* ) {
		mon->checkup_repo_branch(branch);
	}
}

void Monitor::checkup_repo_branch( repo_branch *branch )
{
	stringbuf sa, sb;
	repository *repo = branch->repo;

	sa.printf("http://api.github.com/repos/%s/%s/commits", repo->owner, repo->name);
	Curlpage *cp = curl->Get(sa.p, monitor_checkup_repo_branch_cb);
	sb.printf("sha=%s", branch->name);
	cp->attachdata( sb.p );
	cp->userdata = (void*)branch;
	cp->open();

}

void monitor_checkup_repo_branch_cb( Curler *c, Curlpage *cp )
{
	stringbuf sa;

	//! process
	//! get sha1 for branch commit
	lprintf("Repo branch list:\n%s", cp->contents->p);

	return;
}


void Monitor::close_process_pipe( runprocess *rp )
{
	dlog(LOG_FIVE, "cpp(%p)", rp);

	if( !rp->shellproc ) return;
	rp->shellproc->close();
	tnode *n = pipes->Pull(rp->shellproc);
	delete n;
	delete rp->shellproc;
	rp->shellproc = NULL;
}








// QueueCommand: run a command on the utility pipeline
// this queue is also executed by mainCmdHandler
void Monitor::QueueCommand( const char *cmd, void *data, void *data2 )
{
	if( !strstr(cmd, "\n") ) {
		char *tmx = (char*)malloc(strlen(cmd)+2);
		strcpy(tmx,cmd);
		strcat(tmx,"\n");
		QueueCommand(tmx,data,data2);
		free(tmx);
		return;
	}
	if( mainpipe_ready ) {
		dlog( LOG_SIX, "QC Run %s", cmd );
		mainpipe_ready = false;
		mainpipe->write(cmd);
		//printf("Command to mainpipe: %s", cmd);
		cmd_data = data;
		repo_data = data2;
	} else {
		dlog( LOG_SEVEN, "QC Stall %s", cmd );
		cmdqueue->PushBack( strdup(cmd) );
		cmdqueue->PushBack( data );
		cmdqueue->PushBack( data2 );
	}
}

// Report which pids we are tracking and how much CPU they use to the watch.
void Monitor::ReportPids( void )
{
	monitor_item *item;
	runprocess *rp;
	pid_data *pd;
	tnode *n, *n2, *n3;
	float x;

	dlog( LOG_THREE, "reportpids" );
	FILE *fp = fopen("/tmp/monitor.pids", "w");
	if( !fp ) {
		lprintf("Cannot report monitor pids.");
		return;
	}

	pids_need_update=false;
	forTLIST( item, n, items, monitor_item* ) {
		forTLIST( rp, n2, item->processes, runprocess* ) {
			forTLIST( pd, n3, rp->pids, pid_data* ) {
				x = pd->per_u_10s + pd->per_s_10s;
				//printf("%d:", pd->pid);
				//printf("%.3f:", x);
				//lprintf("%s", pd->procname);
				//lprintf("%d:%.3f:%s", pd->pid, x, pd->procname);
				fprintf(fp, "%d:%.3f:%s\n", pd->pid, x, pd->procname);
			}
		}
	}

	fclose(fp);
}

/*
 *
// Report configuration and current states
void Monitor::ReportStatus( void )
{
	monitor_item *item;
	runprocess *rp;
	logfile *lf;
	pid_data *pd;
	tnode *n, *n2, *n3;
	float x, y, z;
	tlist *q;
	char *buf;
	tlist *alist;

	//lprintf("ReportStatus");
	FILE *fp = fopen("/tmp/monitor.status", "w");

	forTLIST( item, n, items, monitor_item* ) {
		fprintf(fp, "group %s\n", item->name);
		fprintf(fp, "state %d\n", item->state);
		if( item->mainlog )
			fprintf(fp, "logto %s\n", item->mainlog );
		fprintf(fp, "logcount %ld\n", item->cur_logfile_count);
		fprintf(fp, "logsize %ld\n", item->cur_logfile_size);

		forTLIST( buf, n2, item->requires, char* ) {
			fprintf(fp, "require %s\n", buf);
		}

		q = new tlist;
		q->PushBack( item->logfiles );
		while( q->nodes ) {
			alist = (tlist*)q->FullPop();

			forTLIST( lf, n2, alist, logfile* ) {
				if( lf->dirfiles ) {
					fprintf(fp, "logdir %s\n", lf->path);
					q->PushBack(lf->dirfiles);
				} else {
					fprintf(fp, "logfile %s\n", lf->path);
				}
			}
		}
		delete q;

		forTLIST( rp, n2, item->processes, runprocess* ) {
			fprintf(fp, "process %s\n", rp->name);
			fprintf(fp, "state %d %d\n", rp->runstate, rp->cmdstate);
			fprintf(fp, "lastdown %ld\n", rp->last_downtime_start);
			fprintf(fp, "start %ld\n", rp->start_time);
			fprintf(fp, "mainpid %d\n", rp->pid);
			if( rp->cwd && *rp->cwd )
				fprintf(fp, "cwd %s\n", rp->cwd);
			if( rp->env && *rp->env )
				fprintf(fp, "env %s\n", rp->env);
			if( rp->psgrep )
				fprintf(fp, "psgrep %s\n", rp->psgrep);
			forTLIST( pd, n3, rp->pids, pid_data* ) {
				x = pd->per_u_min + pd->per_s_min;
				y = pd->per_u_30s + pd->per_s_30s;
				z = pd->per_u_10s + pd->per_s_10s;
				//lprintf("%d:", pd->pid);
				//lprintf("%.3f:", x);
				//lprintf("%s", pd->procname);
				//lprintf("%d:%.3f:%s", pd->pid, x, pd->procname);
				fprintf(fp, "pid %d %.3f %.3f %.3f %s\n", pd->pid, x, y, z, pd->procname);
			}
		}
	}

	fclose(fp);
}
 */

// Look for a process via the utility pipeline
void Monitor::psgrep_process( runprocess *rp )
{
	stringbuf *sb = new stringbuf();
	dlog( LOG_THREE, "psgrep" );
	rp->last_psgrep_check = time_now;
	rp->cmdstate = MON_CMD_PSGREP;
	sb->printf("ps aux | egrep '%s' | grep -v grep | awk '{printf \"%%s:\", $2; for(i=11;i<=NF;i++){printf \"%%s \", $i}; printf \"\\n\";}'", rp->psgrep);
	QueueCommand(sb->p, (void*)rp);
	delete sb;
}

// Check on a service via the utility pipeline
void Monitor::service_status( runprocess *rp )
{
	stringbuf *sb;

	dlog( LOG_THREE, "service_status(%p)", rp );
	rp->last_service_check = time_now;
	rp->cmdstate = MON_CMD_STATUS;
	sb = new stringbuf();
	sb->printf("service %s status\n", rp->servicename);
	QueueCommand(sb->p, (void*)rp);
	delete sb;
}























/* Callbacks for handling process data and main pipe responses */


bool pipeTest( Pipe *p )
{
	return false;
}


void monitorReadHandler( Pipe *p, char *buf )
{
	runprocess *rp = (runprocess*)p->data;
	if( !rp ) return;
	monitor_item *item = rp->monitem;
	Monitor *m = item->m;

	//! detect errors?
	//! cyclic processing loop?

	dlog(LOG_FIVE, "monitorReadHandler");

	if( rp->cmdstate == MON_CMD_SCANNER ) {
		// find pid, should be first line of readbuffer

		rp->pid = atoi(strstr(buf," "));
		m->Lprintf(rp, "Pid found: '%d'", rp->pid);
		if( rp->pid != 0 ) {
			rp->runstate = MON_STATE_RUNNING;
			rp->cmdstate = MON_CMD_NONE;
		}
	} else if( rp->runstate == MON_STATE_RUNNING ) {
		//! sense any output errors (eg "SEGMENTATION FAULT")
		// route log data to loggly
		struct timeval tv_now;

		tlist *lines = split(buf, "\n");
		tnode *n;
		char *oneline;

		if( m->logglyKey && m->logglyTag ) {
			forTLIST( oneline, n, lines, char* ) {
				gettimeofday( &tv_now, NULL );
				tv_now.tv_sec %= 1000;
				m->Lprintf( item, "%ld.%06ld %s %s", tv_now.tv_sec, tv_now.tv_usec, item->name, oneline);
			}
		}

		//lprintf("[loggly:%d]", lines->count);
		lines->Clear(free);
		delete lines;
	} else if( rp->runstate == MON_STATE_CHANGED ) { // indicates possible process crash, but it's not handled here

	}

}

bool monitorTestProcess( Pipe *p )
{
	runprocess *rp = (runprocess*)p->data;
	if( !rp ) return true;
//	monitor_item *item = rp->monitem;
//	Monitor *m = item->m;

	dlog( LOG_FIVE, "monitorTestProcess" );

	//! detect errors?
	//! cyclic processing loop?

	if( rp->runstate == MON_STATE_CRASHED ) {
		return true;
	}

	return false;
}


void handleCrashed( monitor_item *itx )
{
}

bool monitorIdleHandler( Pipe *p )
{
	//! detect idle time
	//! possibly error out, or just take note of the elapsed time
	// ... ;)
	return true;
}

bool monitorCmdHandler( Pipe *p )
{
	runprocess *rp = (runprocess*)p->data;
	if( !rp ) {
		lprintf("Command handler with no command!");
		return true;
	}
	monitor_item *item = rp->monitem;
	Monitor *m = item->m;

	dlog(LOG_FIVE, "monitorCmdHandler(%p)", p);

	//lprintf("%s: %d", rp->name, rp->runstate);

	switch( rp->runstate ) {
	case MON_STATE_INIT: // startup
		if( rp->cwd && str_cmp(rp->cwd, p->cwd) != 0 ) {
			// wrong cwd
			char buf[1024], matchwild=false;

			if( *p->cwd == '~' ) {
				if( str_cmp(p->username, "root") == 0 ) {
					sprintf(buf, "/root/%s", p->cwd+2);
				} else {
					sprintf(buf, "/home/%s/%s", p->username, p->cwd+2);
				}
				if( str_cmp(rp->cwd, buf) == 0 ) {
					matchwild=true;
				}
				m->Lprintf(rp, "Adjusted directory: %s - %s", p->cwd, buf);
			}

			if( !matchwild ) {
				sprintf(buf, "cd %s\n", rp->cwd);
				p->write( buf );
				m->Lprintf(rp, "Changing directory...");
				break;
			}
		}
		if( rp->keeprunning || rp->autostart ) {
			p->write( rp->startcmd );
			if( rp->use_shellpid ) {
				p->write( " & fg");
				rp->cmdstate = MON_CMD_SCANNER; // read process id
			} else {
				rp->cmdstate = MON_CMD_NONE;
			}
			p->write( "\n" );
			rp->runstate = MON_STATE_RUNNING;
			rp->start_time = time(NULL);
		}
		m->Lprintf(rp, "Sending shell process start command");
		break;
	default: // we shouldn't get here; indicates the command probably exitted
		m->Lprintf(rp, "Got shell commandline during runtime");
		if( rp->dubious ) { // verified: we are at the command line.
			if( !rp->noshell ) { // todo: detect shell working echo?
				p->write( "echo Process crashed\nexit\n" );
			}
			m->Lprintf(rp, "State %d", rp->runstate);
			m->LogCrash(rp, "[commandline]");
			return false;
		} else {
			rp->dubious = true;
			p->write("\n");
		}
		break;
	}
	return true;
}



/* Main shell processing */

void mainReadHandler( Pipe *p, char *buf )
{
	Monitor *m = (Monitor*)p->data;

	dlog(LOG_FIVE, "mainReadHandler(%p)", p);
	if( !m ) {
		lprintf("Lost main pipe control.");
		exit(-1);
	}

	if( m->cmd_data ) {
		runprocess *rp = (runprocess*)m->cmd_data;

		if( rp->cmdstate == MON_CMD_SCANNER ) {
			//! find pid, should be first line of readbuffer
			lprintf("Get pid from: '%s'", buf);

			rp->runstate = MON_STATE_RUNNING;
			rp->cmdstate = MON_CMD_NONE;
		} else if( rp->runstate == MON_STATE_RUNNING ) {
			//! sense any output errors ("SEGMENTATION FAULT, etc)
		}
	}
}

bool mainCmdHandler( Pipe *p )
{
	Monitor *m = (Monitor*)p->data;
	char *buf = p->readbuffer->p;


	dlog( LOG_FIVE, "mainCmdHandler(%p)", p );

	if( !m ) {
		lprintf("Lost main pipe control.");
		return false;
	}

	if( m->repo_data ) {

	}


	if( m->cmd_data != NULL) {
		//lprintf("cmd_data");
		runprocess *rp = (runprocess*)m->cmd_data;
		if( rp->runstate == MON_STATE_INIT ) {
			rp->runstate = MON_STATE_RUNNING;
			rp->cmdstate = MON_CMD_NONE;
			if( rp->monitem ) {
				m->Lprintf( rp->monitem, "Service started.");
			}
		} else if( rp->cmdstate == MON_CMD_STATUS ) {
			if( strstr(buf, "(dead)") ) {
				m->Lprintf(rp, "Service crashed.");
				m->LogCrash( rp, "[service status]" );
			} else if( !strstr(buf, "(running)") ) {
				m->Lprintf( rp, "Service has invalid status: '%s'", buf);
				m->LogCrash( rp, "[service status-2]" );
			} else {
				//report_runprocess(rp); //! trigger heartbeat

			}
			rp->cmdstate = MON_CMD_NONE;
		} else if( rp->cmdstate == MON_CMD_PSGREP ) {
			//lprintf("Psgrep got:\n%s", buf);
			tlist *lines = split(buf, "\n");
			tnode *n, *nn;
			char *line;

			int i=0,newpid;
			pid_data *pd;
			tlist *strings;
			char *x;

			if( lines->count < 1 ) {
				m->Lprintf(rp, "Psgrep not found: '%s'", rp->psgrep);
				rp->psgrep_missing++;
				if( rp->psgrep_missing > 3 ) {
					m->Lprintf( rp, "Psgrep failure condition (3 strikes)." );
					m->LogCrash( rp, "[psgrep]" );
				}
			} else {
				rp->psgrep_missing=0;
			}

			DMap *dm_prev = new DMap(32), *dm_current = new DMap(32);

			// Compile found list to hash table
			forTLIST( line, n, lines, char* ) {
				strip_newline(line);
				strings = split(line, ":");
				if( strings->count < 2 ) {
					m->Lprintf(rp, "Invalid ps line: %s", line);
					continue;
				}
				x = (char*)strings->FullPop();
				newpid = atoi(x);
				free(x);
				x = join(strings, ":");
				dm_current->Set(newpid, x);
				//lprintf("ps:%d:%s", newpid, x);
				strings->Clear( free );
				delete strings;
			}
			lines->Clear(free);
			delete lines;

			tlist *sc498 = dm_current->ToList( NULL, true );

			// Check expected pids against found list
			forTSLIST( pd, n, rp->pids, pid_data*, nn ) {
				if( !dm_current->Get( pd->pid ) ) {
					rp->pids->Pull(n);
					m->Lprintf(rp, "Process exited: %d", pd->pid);
					free_pid_data(pd);
				} else {
					dm_prev->Set( (uint32_t)pd->pid, (void*)pd );
				}
			}

			ivoid *np;

			// Check found list for new pids
			forTLIST( np, n, sc498, ivoid* ) {

				x = (char*)np->ptr;
				pd = (pid_data*)dm_prev->Get(np->idc);
				if( !pd ) {
					m->Lprintf(rp, "New pid %u", np->idc);
					pd = new_pid_data();
					pd->pid = np->idc;
					pd->procname = strdup(x);
					rp->pids->Push( pd );
					if( rp->pid <= 0 ) rp->pid = np->idc;
				}

				if( i == 0 && rp->pid <= 0 )
					rp->pid = pd->pid;
				i++;
			}
			delete dm_prev;
			dm_current->Clear( free );
			delete dm_current;

			rp->cmdstate = MON_CMD_NONE;

			m->pids_need_update = true;
		}
//	} else {
//		lprintf("no cmd_data");
	}

	if( m->cmdqueue->count <= 0 ) {
		m->mainpipe_ready = true;
		m->cmd_data = NULL;
		m->repo_data = NULL;
		//lprintf("Mainpipe ready.");
	} else {
		m->mainpipe_ready = false;

		char *cmd = (char*)m->cmdqueue->FullPop();
		m->cmd_data = m->cmdqueue->FullPop();
		m->repo_data = m->cmdqueue->FullPop();
		p->write( cmd );
		//printf("queue-run[%d]: %s", m->cmdqueue->count/2, cmd);
		free(cmd);
	}
	return true;
}


















/* Loop Management Functions */
// FD_RESET: set flags for each monitored service
int Monitor::FD_RESET( fd_set *ins, fd_set *excepts )
{
//	monitor_item *item;
	tnode *n;
//	logfile *lf;
//	tnode *nn;
	Pipe *p;
	int maxfd=0, cp_maxfd;

	dlog( LOG_ZERO, "fd_reset" );

	if( inotify_fd >= 0 ) {
		FD_SET( inotify_fd, ins );
		FD_SET( inotify_fd, excepts );
		maxfd = inotify_fd+1;
	}
	if( mainpipe ) {
		mainpipe->FD_TSET( ins, excepts );
		if( mainpipe->fdm >= maxfd )
			maxfd = mainpipe->fdm+1;
	}
	if( cmdpipe ) {
		cp_maxfd = cmdpipe->FD_RESET(ins,excepts);
		if( cp_maxfd >= maxfd )
			maxfd = cp_maxfd + 1;
	}

	forTLIST( p, n, pipes, Pipe* ) {
		p->FD_TSET( ins, excepts );
		if( p->fdm >= maxfd )
			maxfd = p->fdm+1;
	}

	return maxfd;
}



// Process monitored flags that have turned up
bool Monitor::FD_CHECK( fd_set *ins, fd_set *excepts )
{
//	monitor_item *item;
	tnode *n, *nn;
//	tnode *nn;
	logfile *lf;
	watchfile *wf;
	Pipe *p;
	char ibuf[IBUF_LEN];
	bool found=false;
	int len, i;

	time_now = time(NULL);
	dlog( LOG_ZERO, "fd_check" );

	if( pids_need_update ) {
		ReportPids();
	}

	if( cmdpipe ) {
		cmdpipe->FD_CHECK(ins,excepts);
	}

	if( inotify_fd >= 0 ) {
		if( FD_ISSET( inotify_fd, ins ) ) {
			//lprintf("!!!! INOTIFY !!!! inotify events reported");
			dlog( LOG_SIX, "Read inotify events" );
			found=true;
			len = ::read( inotify_fd, ibuf, IBUF_LEN );
			struct inotify_event *ev;

			if( len <= 0 ) {
				lprintf( "EOF on logfile." );
			} else {
				i=0;
				do {
					ev = (struct inotify_event*)&ibuf[i];

					i += EVENT_SIZE + ev->len;
					lf = (logfile*)logfiles->Get(ev->wd);
					if( !lf ) {
						wf = (watchfile*)watchfiles->Get(ev->wd);
						if( !wf ) {
							lprintf("Can't read inotify watch info %d", ev->wd);
						} else {
							changed_watchfile(wf);
						}
					} else {
						changed_logfile(lf);
					}
					/*
					forTLIST( item, n, items, monitor_item* ) {
						forTLIST( lf, nn, item->logfiles, logfile* ) {

						}
					}*/
				} while( i < len );
			}
		}
	}

	char buf[1025];

	if( mainpipe ) {
		i = mainpipe->FD_TEST( ins, excepts );
		if( i == -1 ) {
			found=true;
			lprintf( "Main pipe error." );
		} else if( i == 1 ) {
			found=true;
			len = mainpipe->FD_READ(buf, 1024);
			if( len <= 0 ) {
				lprintf( "Main pipe closed." );
				exit(-1);
			} else {
				//! check for any errors in buf
				if( strstr(buf, "systemctl daemon-reload") ) {
					lprintf("Need to reload daemon.");
					mainpipe->write("systemctl daemon-reload\n");
				}
			}
		}
	}

	runprocess *rp;

	forTSLIST( p, n, pipes, Pipe*, nn ) {
		rp = (runprocess*)p->data;
		i = p->FD_TEST( ins, excepts );
		if( i == -1 ) {
			found=true;
			Lprintf(rp, "Runtime pipe for process %s errored.", rp->startcmd );
		} else if( i == 1 ) {
			found=true;
			len = p->FD_READ( buf, 1024 );
			if( len <= 0 ) {
				Lprintf( rp, "Pipe for %s closed.", rp->startcmd );
				if( rp->runstate != MON_STATE_CRASHED ) {
					LogCrash(rp, "[pipe closed]");
				}
			} else {
				//! check for any errors in buf
			}
		}
	}

	return found;
}


runprocess *Monitor::GetProcess( monitor_item *item, const char *procname )
{
	tnode *n;
	runprocess *rp;

	forTLIST( rp, n, item->processes, runprocess* ) {
		if( strcmp(procname, rp->name) == 0 ) {
			return rp;
		}
	}

	return NULL;
}



// Scan through all services and verify status
void Monitor::Iterate( void )
{
	// Check each iterateable process
	monitor_item *item;
	tnode *n, *nn;

	time_now = time(NULL);
	dlog( LOG_THREE, "iterate" );

	if( pids_need_update ) {
		ReportPids();
	}

	// check if web is online
	if( !web_online || number_range(0,30) > 26 ) {
		struct stat fstatdata;

		if( stat( "/tmp/monitor.web", &fstatdata ) == 0 ) {
			time_t time_last = fstatdata.st_mtime;
			web_online = ( time_now - time_last < 300 ); // 5*60
		}
	}

	forTSLIST( item, n, items, monitor_item*, nn ) {
		if( !item->paused )
			Checkup(item);
		else if( time_now >= item->start_time ) {
			item->paused = false;
			item->start_time = -1;
			Start(item);
		}
	}

	if( pids_need_update || web_online ) {
		ReportPids();
		//ReportStatus();
	}
}

// Check on a group's status. This means updating logfiles, scanning pids online, and detecting CPU usage.
bool Monitor::Checkup( monitor_item *item )
{
	tnode *n;
	logfile *lf;
	watchfile *wf;
	runprocess *rp;
	bool foundChanges = false;

	time_t current_time = time(NULL);
	dlog( LOG_THREE, "checkup" );

	if( item->start_time <= current_time ) {
		forTLIST( rp, n, item->processes, runprocess* ) {
			if( rp->paused && rp->start_time <= current_time ) {
				foundChanges = true;
				rp->paused = false;
			}
		}
		if( item->start_time <= 0 ) {
			Lprintf(item, "Group not yet started, trying to start it now.");
			item->paused=false;
			Start(item);
			return true;
		} else if( foundChanges ) {
			Lprintf(item, "Group paused, trying to start it now.");
			item->paused=false;
			Start(item);
			return true;
		}
	}

	forTLIST( lf, n, item->logfiles, logfile* ) {
		if( checkup_logfile(lf) ) {
			foundChanges=true;
		}
	}
	forTLIST( wf, n, item->watchfiles, watchfile* ) {
		if( checkup_watchfile(wf) ) {
			foundChanges=true;
		}
	}

	if( item->last_logfile_verify == 0 || ( foundChanges && current_time - item->last_logfile_verify > 600 ) ) { // 10 minutes
		item->last_logfile_verify = current_time;
		verify_logfiles(item);
	}
	forTLIST( rp, n, item->processes, runprocess* ) {
		if( !rp->paused && checkup_process(rp) ) {
			foundChanges=true;
		}
	}

	return foundChanges;
}

void Monitor::verify_logfiles( monitor_item *item )
{
	tnode *n;
	logfile *lf;
	long total_size=0, total_count=0;

	Lprintf(item, "Verifying logfiles.");

	forTLIST( lf, n, item->logfiles, logfile* ) {
		verify_logfile( item, lf, &total_count, &total_size );
	}

	item->cur_logfile_size = total_size;
	item->cur_logfile_count = total_count;

	if( ( item->max_logfile_size != 0 && item->cur_logfile_size > item->max_logfile_size ) ||
		( item->max_logfile_count != 0 && item->cur_logfile_count > item->max_logfile_count ) ) {
		trim_logfiles(item);
	}
}

void Monitor::trim_logfiles( monitor_item *item )
{
	Lprintf(item, "Recycling logfiles. Current files: %ld size: %ld", item->cur_logfile_count, item->cur_logfile_size);
	//! todo: delete oldest files first
	//! reach trim_logfile_count and then reach trim_logfile_size
	//! use item->log_archive_cmd or 'rm -f'
}

bool Monitor::verify_logfile( monitor_item *item, logfile *lf, long *file_count, long *kb_count )
{
	struct stat fstatdata;

	if( lf->dirfiles ) { // it is a directory. check its subfiles instead
		return verify_logfile_directory(item, lf, lf->path, file_count, kb_count);
	}

	if( stat( lf->path, &fstatdata ) != 0 ) {
		return false;
	}

	*file_count += 1;
	*kb_count += fstatdata.st_size/1024;

	return true;
}

bool Monitor::verify_logfile_directory( monitor_item *item, logfile *lf_item, const char *readpath, long *file_count, long *kb_count )
{
	logfile *lf;
	struct stat st;
	stringbuf subdir;
	bool foundChange=false;

	DIR* odir = opendir(readpath);
	struct dirent* ofile;

	tlist *defer = new tlist;
	char *subpath;

	while( (ofile = readdir(odir)) != NULL ) {

		if( ofile->d_name[0] == '.' ) continue; // leave hidden files hidden
		if( str_cmp(ofile->d_name, "..") == 0 ) continue;
		if( str_cmp(ofile->d_name, ".") == 0 ) continue;

		if( stat(lf->path, &st) == 0 ) {
			continue;
		}

		// Is it a directory?
		if( S_ISDIR(st.st_mode) ) {
			subdir.clear();
			subdir.printf("%s/%s", readpath, ofile->d_name);
			subpath = strdup(subdir.p);
			defer->PushBack(subpath);
			continue;
		}

		if( file_count ) *file_count += 1;

		subdir.clear();
		subdir.printf("%s/%s", readpath, ofile->d_name);
		lf = (logfile*)lf_item->dfdex->Get( subdir.p );
		if( !lf ) {
			// new file
			foundChange=true;

		} else if( kb_count ) {
			*kb_count += st.st_size/1024;
		}
	}
	closedir(odir);

	tnode *n;

	forTLIST( subpath, n, defer, char* ) {
		if( !verify_logfile_directory(item, lf_item, subpath, file_count, kb_count) ) {
			foundChange=true;
		}
	}
	defer->Clear(free);
	delete defer;

	return !foundChange;
}

bool Monitor::checkup_logfile_directory( monitor_item *item, logfile *lf_item, const char *readpath )
{
	logfile *lf;
	struct stat st;
	stringbuf subdir;
	bool foundChange=false;

	DIR* odir = opendir(readpath);
	struct dirent* ofile;

	tlist *defer = new tlist;
	char *subpath;

	while( (ofile = readdir(odir)) != NULL ) {

		if( ofile->d_name[0] == '.' ) continue; // leave hidden files hidden
		if( str_cmp(ofile->d_name, "..") == 0 ) continue;
		if( str_cmp(ofile->d_name, ".") == 0 ) continue;

		if( stat(lf->path, &st) == 0 ) {
			continue;
		}

		// Is it a directory?
		if( S_ISDIR(st.st_mode) ) {
			subdir.clear();
			subdir.printf("%s/%s", readpath, ofile->d_name);
			subpath = strdup(subdir.p);
			defer->PushBack(subpath);
			continue;
		}

		subdir.clear();
		subdir.printf("%s/%s", readpath, ofile->d_name);
		lf = (logfile*)lf_item->dfdex->Get( subdir.p );
		if( !lf ) {
			// new file
			foundChange=true;
			lf = new_logfile();
			lf->path = strdup(subdir.p);
			lf->monitem = item;
			lf_item->dirfiles->PushBack(lf);
			lf_item->dfdex->Set( subdir.p, lf );
			item->cur_logfile_count++;
		} else if( lf->lastsize != st.st_size ) {
			foundChange=true;
			//! do not report here, we will scan the file again later.
		}
	}
	closedir(odir);

	tnode *n;

	forTLIST( subpath, n, defer, char* ) {
		if( checkup_logfile_directory(item, lf_item, subpath) ) {
			foundChange=true;
		}
	}
	defer->Clear(free);
	delete defer;

	return foundChange;
}
// Check on a single logfile
bool Monitor::checkup_logfile( logfile *lf )
{
	monitor_item *item = lf->monitem;

	dlog( LOG_FOUR, "checkup_logfile(%p)", lf );
	if( lf->dirfiles ) { // it is a directory. check its subfiles instead
		bool found_results=false;
		if( checkup_logfile_directory(item, lf, lf->path) ) // index new files
			found_results=true;
		tnode *n;
		logfile *lf_item;

		forTLIST( lf_item, n, lf->dirfiles, logfile* ) {
			if( checkup_logfile(lf_item) ) {
				found_results=true;
			}
		}
		return found_results;
	}

	if( inotify_fd >= 0 ) {
		if( lf->watchdesc < 0 ) {
			logfile_add_scanner( item, lf );
		}
	} else {
		if( lf->watchdesc >= 0 ) {
			lf->watchdesc = -1;
		}
	}
	changed_logfile(lf);

	return false;
}
void Monitor::changed_logfile( logfile *lf )
{
	FILE *fp;
	struct stat fstatdata;
	monitor_item *item = lf->monitem;

	if( stat( lf->path, &fstatdata ) == 0 ) {
		if( fstatdata.st_size != lf->lastsize ) {
			dlog( LOG_FOUR, "changed_logfile(%p)", lf );
			if( lf->lastsize > fstatdata.st_size ) {
				lf->lastsize = 0;
			}
			//* Read new content in logfile
			int readlen = ( fstatdata.st_size - lf->lastsize );
			char *readbuf = (char*)malloc( readlen + 1 );
			fp = fopen(lf->path, "r");
			fseek(fp, lf->lastsize, SEEK_SET);
			readlen = fread(readbuf, 1, readlen, fp);
			readbuf[readlen] = 0;
			fclose(fp);

			//* Append to current log size
			item->cur_logfile_size += readlen;

			if( item->mainlog ) {

				//* Redirect log content to main log file
				fp = fopen(item->mainlog, "a");
				if( fp ) {
					fputs(readbuf, fp);
					fclose(fp);
				} else {
					lprintf("Cannot write to '%s': %s", item->mainlog, readbuf);
				}
			}

			if( ( logglyKey || item->logglyKey ) && ( logglyTag || item->logglyTag ) ) {
				//* Direct information to Loggly
				struct timeval tv_now;

				tlist *lines = split(readbuf, "\n");
				tnode *n;
				char *oneline;

				forTLIST( oneline, n, lines, char* ) {
					gettimeofday( &tv_now, NULL );
					tv_now.tv_sec %= 1000;
					Lprintf( item, "%ld.%06ld %s %s", tv_now.tv_sec, tv_now.tv_usec, item->name, oneline);
				}

				lines->Clear(free);
				delete lines;
			}

			free(readbuf);


			lf->lastsize = fstatdata.st_size;

		}
	}
}
void Monitor::changed_watchfile( watchfile *lf )
{
	monitor_item *item = lf->monitem;

	dlog( LOG_FOUR, "changed_watchfile(%p)", lf );
	if( inotify_fd >= 0 ) {
		if( lf->watchdesc < 0 ) {
			Lprintf( item, "watchfile: add inotify [%s]", lf->path);
			watchfile_add_scanner( item, lf );
		}
	} else {
		Lprintf( item, "watchfile: no ready inotify controller" );
		if( lf->watchdesc >= 0 ) {
			lf->watchdesc = -1;
		}
	}

	if( !lf->action || str_cn_cmp(lf->action, "restart") == 0 ) {
		Restart(); // restart all groups
	} else { // run a system command
		char buf[4096];
		sprintf(buf, "%s %s", lf->action, lf->path);
		QueueCommand( buf, NULL );
	}
	return;
}
bool Monitor::checkup_watchfile( watchfile *lf )
{
	monitor_item *item = lf->monitem;

	dlog( LOG_FOUR, "checkup_watchfile(%p)", lf );
	if( inotify_fd >= 0 ) {
		if( lf->watchdesc < 0 ) {
			Lprintf( item, "watchfile: add inotify [%s]", lf->path);
			watchfile_add_scanner( item, lf );
		}
	} else {
		Lprintf( item, "watchfile: no ready inotify controller" );
		if( lf->watchdesc >= 0 ) {
			lf->watchdesc = -1;
		}
	}
	return false;
}

// Check on a single process or service
// This function's context is sensitive to state variables and may fail if invoked improperly
bool Monitor::checkup_process( runprocess *rp )
{
	tnode *n, *nn;
	pid_data *pd;
	bool shell_online=false;

	dlog( LOG_FOUR, "checkup_process(%p)", rp );

	if( rp->runstate != MON_STATE_NONE && rp->runstate != MON_STATE_INIT && rp->runstate != MON_STATE_RUNNING ) {
		lprintf("Unexpected state %d.", rp->runstate);
		return true;
	}

	// when process starts, locate pids:
	if( rp->runstate == MON_STATE_RUNNING && rp->psgrep && rp->cmdstate == MON_CMD_NONE && ( rp->pid <= 0 || rp->pids->count == 0 ) ) {
		lprintf("Initial Psgrep: %s", rp->psgrep);
		psgrep_process(rp);
		return true;
	}

	// check to make sure main pid is online:
	if( rp->pid > 0 ) {
		if( !pid_is_online( rp->pid ) ) {
			Lprintf(rp, "Pid offline %d", rp->pid);
			rp->pid = 0;
			pids_need_update=true;
			// process died
			// try a psgrep - maybe it switched ids while we weren't looking
			psgrep_process(rp);
			return false;
		} else {
			shell_online=true;
		}
	}

	// check to make sure pipe is still running:
	if( rp->shellproc ) {
		if( !pid_is_online(rp->shellproc->childproc) ) {
			LogCrash(rp, "Shell pipe pid offline");
			return false;
		} else if( rp->shellproc->fdm < 0 ) {
			LogCrash(rp, "Shell pipe offline");
			return false;
		}
		shell_online = true;
	}

	// read stats on the pids:
	if( rp->pids->count > 0 ) {
		pids_need_update = true;
		rp->last_pid_check = time_now;
		forTSLIST( pd, n, rp->pids, pid_data*, nn ) {
			if( !checkup_pid( rp, pd ) ) {
				if( pd->pid == rp->pid ) {
					Lprintf(rp, "Main pid not found");
					rp->pid = 0;
				}
				free_pid_data(pd);
				rp->pids->Pull(n);
			}
		}
	}

	// check service status:
	if( rp->cmdstate == MON_CMD_NONE && rp->servicename && rp->runstate == MON_STATE_RUNNING ) {
		if( (time_now - 5) > rp->last_service_check ) {
			service_status(rp);
		}
		shell_online = true;
	}

	// re-run psgrep to update current pids:
	if( rp->cmdstate == MON_CMD_NONE && rp->psgrep && rp->runstate == MON_STATE_RUNNING ) {
		if( (time_now - 10) > rp->last_psgrep_check ) {
			psgrep_process(rp);
		}
		shell_online = true;
	}

	return shell_online;
}


// Scan a known pid and get all of its CPU data
bool Monitor::checkup_pid( runprocess *rp, pid_data *pid )
{
	pid_record *pr;

	dlog( LOG_FOUR, "checkup_pid(%p)", rp );

	if( !pid_is_online( pid->pid ) )
		return false;

	pr = get_pid_record(pid->pid);
	if( pr != NULL ) {
		pid->minute->Push(pr);
		recalculate_pid_cpu(rp, pid);
		pids_need_update=true;
		return true;
	} else {
		Lprintf(rp, "Couldn't get process stats for %d.", pid->pid);
	}
	return false;
}

// Adjust calculations of CPU based on historical usage
// also discard old records
void Monitor::recalculate_pid_cpu( runprocess *rp, pid_data *pid )
{
	struct timeval tv_now;
	pid_record *pr;
	tnode *n, *nn;
	long minx;

	struct timeval ut_zero, st_zero, at_zero;
	long per_100;

	long per_10000, per_10000_u, per_10000_s;
	long per_30000, per_30000_u, per_30000_s;
	long per_min, per_min_u, per_min_s;
	long min_ut, min_st;
	bool zero_found=false;

	dlog( LOG_FOUR, "recalculate_pid_cpu(%p)", rp );

	per_100 = min_ut = min_st = 0;
	per_10000 = per_10000_u = per_10000_s = 0;
	per_30000 = per_30000_u = per_30000_s = 0;
	per_min = per_min_u = per_min_s = 0;

	ut_zero.tv_sec = ut_zero.tv_usec = 0;
	st_zero.tv_sec = st_zero.tv_usec = 0;
	at_zero.tv_sec = at_zero.tv_usec = 0;

	gettimeofday( &tv_now, NULL );

	for( n = pid->minute->last; n; n = nn ) {
		nn = n->prev;
		pr = (pid_record*)n->data;

		minx = tv_diff( &tv_now, &pr->at );
		if( minx > 1000 * 60 ) {
			free_pid_record(pr);
			pid->minute->Pull(n);
			continue;
		}

		if( !zero_found ) {
			zero_found = true;

			tv_copy( &ut_zero, &pr->utime );
			tv_copy( &st_zero, &pr->stime );
			tv_copy( &at_zero, &pr->at );
		} else {
			min_ut = tv_diff( &pr->utime, &ut_zero );
			min_st = tv_diff( &pr->stime, &st_zero );
			per_100 = tv_diff( &pr->at, &at_zero );
			if( per_100 < 10000 ) {
				per_10000 += per_100;
				per_10000_u += min_ut;
				per_10000_s += min_st;
			} else if( per_100 < 30000 ) {
				per_30000 += per_100;
				per_30000_u += min_ut;
				per_30000_s += min_st;
			} else {
				per_min += per_100;
				per_min_u += min_ut;
				per_min_s += min_st;
			}
		}
	}

	pid->ut_10s = per_10000_u;
	if( per_10000 != 0 ) {
		pid->per_u_10s = (100.0*(float)per_10000_u) / (float)per_10000;
		pid->per_s_10s = (100.0*(float)per_10000_s) / (float)per_10000;
	} else {
		pid->per_u_10s = pid->per_s_10s = 0;
	}
	if( per_30000 != 0 ) {
		pid->per_u_30s = (100.0*(float)per_30000_u) / (float)per_30000;
		pid->per_s_30s = (100.0*(float)per_30000_s) / (float)per_30000;
	} else {
		pid->per_u_30s = pid->per_s_30s = 0;
	}
	if( per_min != 0 ) {
		pid->per_u_min = (100.0*(float)per_min_u) / (float)per_min;
		pid->per_s_min = (100.0*(float)per_min_s) / (float)per_min;
	} else {
		pid->per_u_min = pid->per_s_min = 0;
	}
}













// Start everything we need.
void Monitor::Start( void )
{
	// Check each iterateable process
	monitor_item *item;
	tnode *n;

	dlog( LOG_ONE, "start" );

	time_now = time(NULL);

	forTLIST( item, n, items, monitor_item* ) {
		if( !Start(item) ) {
			Lprintf(item, "Start was unsuccessful.");
			//! report errors
		}
	}
}

// Monitor all logfiles and start all processes and services in a group
bool Monitor::Start( monitor_item *item )
{
	tnode *n;
	logfile *lf;
	watchfile *wf;
	runprocess *rp;
	char *require;
	tnode *n2;
	monitor_item *tgt;
	bool found=false;
	int warmup=0;
	time_t timeNow = time(NULL);

	if( item->start_time <= timeNow ) {
		if( item->paused ) {
			item->paused = false;
			item->stage2 = true; // process has been restarted
		}
	}

	if( item->paused ) {
		Lprintf(item, "Process is paused; not starting.");
		return false;
	}

	dlog( LOG_THREE, "start(%p)", item );
	Lprintf(item, "Monitor::Start");

	if( item->state != MON_STATE_NONE ) item->state=MON_STATE_NONE;//return true; // already monitoring

	item->start_time = 0;

	forTLIST( require, n2, item->requires, char* ) {
		found=false;
		forTLIST( tgt, n, items, monitor_item* ) {
			if( str_cmp(require, tgt->name) == 0 ) {
				found=true;
				if( tgt->started_time < tgt->start_time || tgt->start_time == 0 || tgt->started_time >= timeNow ) {
					Lprintf(item, "Monitor::require(%s) not met\n", require);
					return false;
				}
				break;
			}
		}
		if( !found ) {
			Lprintf(item, "Monitor::require(%s) not found\n", require);
			return false;
		}
	}

	item->start_time = time(NULL);
	forTLIST( lf, n, item->logfiles, logfile* ) {
		start_logfile(item, lf);
	}
	forTLIST( wf, n, item->watchfiles, watchfile* ) {
		start_watchfile(item, wf);
	}
	forTLIST( rp, n, item->processes, runprocess* ) {
		if( rp->autostart && !rp->paused && !start_process(item, rp) ) {
			Lprintf(rp, "Couldn't start process %s", rp->startcmd);
			return false;
		} else {
			warmup += rp->warmup;
		}
	}

	item->started_time = time(NULL) + warmup;
	Lprintf(item, "Start::Done");
	return true;
}

// Monitor a single logfile
//! todo: or directory
bool Monitor::start_logfile( monitor_item *item, logfile *lf )
{
	//* Get initial size and statistics
	struct stat st;
	if( stat(lf->path, &st) == 0 ) {
		lf->lastsize = st.st_size;
	} else {
		lf->lastsize = 0;
	}

	//* Check if it is a directory
	// Is it a directory?
	if( S_ISDIR(st.st_mode) ) {
		lf->watchdesc = -1;
		lf->monitor_inotify_fd = -1;
		lf->lastsize = -1;
		start_logfile_directory(item, lf, lf->path);
		return true;
	}

	dlog( LOG_FOUR, "start_logfile(%p, %p)", item, lf );
	if( !lf->valid && inotify_fd >= 0 ) {
		lprintf("start_logfile(%s)", lf->path);
		lf->monitor_inotify_fd = inotify_fd;
		lf->watchdesc = inotify_add_watch( inotify_fd, lf->path, IN_MODIFY | IN_CREATE | IN_DELETE );
		logfiles->Set( lf->watchdesc, (void*)lf );
	} else {
		lf->watchdesc = -1;
		lf->monitor_inotify_fd = -1;
	}

	return true;
}

void Monitor::start_logfile_directory( monitor_item *item, logfile *lf_item, const char *readpath )
{
	logfile *lf;
	struct stat st;
	stringbuf subdir;

	DIR* odir = opendir(readpath);
	struct dirent* ofile;

	if( lf_item->dirfiles != NULL ) {
		Lprintf(item, "Warning: Re-initializing directory listing.");
		delete lf_item->dirfiles;
	}
	lf_item->dirfiles = new tlist();
	lf_item->dfdex = new SMap(32);

	while( (ofile = readdir(odir)) != NULL ) {

		if( ofile->d_name[0] == '.' ) continue; // leave hidden files hidden
		if( str_cmp(ofile->d_name, "..") == 0 ) continue;
		if( str_cmp(ofile->d_name, ".") == 0 ) continue;

		subdir.clear();
		subdir.printf("%s/%s", readpath, ofile->d_name);

		if( stat(subdir.p, &st) == 0 ) {
			continue;
		}

		// Is it a directory?
		if( S_ISDIR(st.st_mode) ) {
			char *tmp;
			tmp = strdup(subdir.p);
			start_logfile_directory(item, lf_item, tmp);
			free(tmp);
			continue;
		}

		lf = new_logfile();
		subdir.clear();
		subdir.printf("%s/%s", readpath, ofile->d_name);
		lf->path = strdup(subdir.p);
		lf->monitem = item;
		lf_item->dirfiles->PushBack(lf);
		lf_item->dfdex->Set( subdir.p, lf );
	}
	closedir(odir);
}


void Monitor::logfile_add_scanner( monitor_item *item, logfile *lf )
{
	lf->watchdesc = inotify_add_watch( inotify_fd, lf->path, IN_MODIFY | IN_CREATE | IN_DELETE );
//! todo: reconfigure w/ watchfile scanner
	//! note: watchfile scanner should include IN_CREATE
}
void Monitor::watchfile_add_scanner( monitor_item *item, watchfile *lf )
{
	int wd;
	tlist *rc;
	struct dirent *de;
	DIR *xd;
	char *srcpath;
	int *wdc;
	stringbuf sb;

	if( !lf->subdirs ) {
		lprintf("start watch %s: no subdirectories", lf->path );
		sb.clear();
		if( lf->path[ strlen(lf->path) - 1 ] != '/' ) {
			sb.printf("%s/", lf->path);
		} else {
			sb.printf("%s", lf->path);
		}
		wd = inotify_add_watch( inotify_fd, sb.p, IN_MODIFY | IN_DELETE | IN_CREATE );
		lprintf("wd1: %d %s", wd, sb.p);
		watchfiles->Set( wd, (void*)lf );
		wdc = (int*)malloc(sizeof(int));
		*wdc = wd;
		lf->watch->PushBack( wdc );
	} else {
		rc = new tlist();

		lprintf("start watch %s: subdirs", lf->path);

		rc->Push( strdup( lf->path ) );
		while( ( srcpath = (char*)rc->FullPop() ) ) {
			//bool subdirs=false;

			lprintf("watch add %s", srcpath);

			xd = opendir( srcpath );
			if( !xd ) {
				lprintf("Couldn't read watchfile path %s", srcpath);
			} else {
				while( ( de = readdir( xd ) ) ) {
					if( de->d_name[0] == '.' ) continue;
					if( ( de->d_type & DT_DIR ) == DT_DIR ) {
						sb.clear();
						sb.printf( "%s/%s", srcpath, de->d_name );
						rc->PushBack( strdup( sb.p ) );
						//subdirs=true;
					}
				}
				closedir(xd);
				sb.clear();
				if( srcpath[ strlen(srcpath)-1 ] != '/' ) {
					sb.printf( "%s/", srcpath );
					lprintf("add / to %s", srcpath);
				} else {
					sb.printf( "%s", srcpath );
					lprintf("found / on %s", srcpath);
				}
				wd = inotify_add_watch( inotify_fd, sb.p, IN_MODIFY | IN_CREATE | IN_DELETE );
				watchfiles->Set( wd, (void*)lf );
				lprintf("wd: %d %s", wd, sb.p);
				wdc = (int*)malloc(sizeof(int));
				*wdc = wd;
				lf->watch->PushBack( (void*)wdc );
			}

			free(srcpath);
		}
		delete rc;
	}
}
bool Monitor::start_watchfile( monitor_item *item, watchfile *lf )
{
	dlog( LOG_FOUR, "start_watchfile(%p, %p)", item, lf );
	lprintf("start_watchfile(%s)", lf->path);

	if( !lf->valid && inotify_fd >= 0 ) {
		this->watchfile_add_scanner(item,lf);
		lf->valid = true;
	} else {
		lprintf("scanner disabled");
		lf->watchdesc = -1;
		lf->valid = false;
	}

	return true;
}
void Monitor::force_start_process( monitor_item *item, runprocess *rp )
{
	rp->paused = false;
	rp->start_tries = 0;
	rp->start_time = 0;
	try {
	if( !start_process(item,rp) ) {
		// oh well....
	}
	} catch(int v) {
		// meh....
	}
}
// Start a single process via fork/exec, service, or shell
bool Monitor::start_process( monitor_item *item, runprocess *rp )
{
	time_t timeNow = time(NULL);
	stringbuf sb;

	if( rp->paused ) {
		Lprintf(rp, "Cannot start: process is paused by command.");
		return false;
	}

	rp->start_tries++;
//	time_t current_time = time(NULL);
//	time_t diff_time = current_time - rp->last_downtime_start;

	if( item->stage2 && rp->start_tries > 3 ) {
		rp->start_tries = 0;
		rp->paused = true;
		if( item->last_fail_startup < timeNow - 60 ) {
			item->start_time = rp->start_time = timeNow + 600;
			Lprintf(rp, "Cannot start: process is not starting up. Waiting ten minutes for restart.");
		} else {
			item->last_fail_startup = timeNow;
			item->start_time = rp->start_time = timeNow + 180;
			Lprintf(rp, "Cannot start: process is not starting up. Waiting three minutes for restart.");
		}
		return false;
	}
	if( rp->start_tries > 6 ) {
		rp->paused = true;
		Lprintf(rp, "Cannot start: process is not starting up. Waiting one minute for restart.");
		// find a logfile and read it
		rp->start_tries = 0;
		item->start_time = rp->start_time = timeNow + 60;
		return false;
	}
	dlog( LOG_FOUR, "start_process(%p, %p)", item, rp );
	Lprintf(rp, "start_process");

	if( rp->servicename ) {
		sb.printf("service %s start\n", rp->servicename);
		QueueCommand( sb.p, (void*)rp );
		sb.clear();
		rp->runstate = MON_STATE_INIT;
		Lprintf(rp, "Starting service %s", rp->servicename);

		return true;
	}
	if( rp->shellproc ) {
		Lprintf(rp, "Closing old shell..");
		close_process_pipe(rp);
	}
	Lprintf(rp, "Opening new shell");
	Pipe *p = new Pipe();
	rp->shellproc = p;
	p->data = (void*)rp;

	if( rp->logtofn ) {
		Lprintf(rp, "Logging to: %s\n", rp->logtofn);
		p->log(rp->logtofn);
	} else if( item->mainlog ) {
		Lprintf(rp, "Logging to: %s", item->mainlog);
		p->log(item->mainlog);
	} else if( item->logglyTag ) {
		Lprintf(rp, "Use loggly tag: %s", item->logglyTag);
		if( item->logglyKey )
			Lprintf(rp, "Use loggly key: %s", item->logglyKey);
	} else {
		Lprintf(rp, "No log assigned");
	}
	p->readCB = monitorReadHandler;
	p->idleCB = monitorIdleHandler;
	p->cmdlineCB = monitorCmdHandler;

	p->breakTest = monitorTestProcess;

	p->write_to_stdout = false;
	p->read_from_stdin = false;

	if( rp->use_newsid ) { // new session
		p->use_newsid = true;
	}

	if( rp->noshell ) {
		Lprintf(rp, "Running command");
		tlist *argsL = split( rp->startcmd, " " );
		char **argsA = argsL->ToArray2<char*>((cloneFunction*)strdup);
		if( rp->cwd ) {
			Lprintf(rp, "Enter directory '%s'", rp->cwd);
			p->chdir(rp->cwd);
		}
		if( rp->env ) {
			p->setenv(rp->env);
		}
		p->run(argsA);
		int i;
		for(i=0;i<argsL->count;i++){
			free(argsA[i]);
		}
		free(argsA);
		delete argsL;

		rp->runstate = MON_STATE_RUNNING;
		rp->pid = p->childproc;
	} else {
		p->open();
		p->getcommandprompt();

		bool sentstart = false;
		sb.clear();
		if( rp->cwd ) {
			sb.printf("cd %s", rp->cwd);
			sentstart=true;
		}
		if( rp->env ) {
			if( sentstart ) sb.append(" && ");
			sb.printf("%s", rp->env);
			sentstart=true;
		}
		if( sentstart ) sb.append(" && ");
		sb.printf("echo\n");
		p->detect_new_cwd = true;

		p->write(sb.p);
		rp->runstate = MON_STATE_INIT;
	}

	pipes->PushBack(p);
	rp->cmdstate = MON_CMD_NONE;
	rp->start_time = time(NULL);

	return true;
}
















bool Monitor::Stop(monitor_item *item)
{
	tnode *n;
	runprocess *rp;
	logfile *lf;
	watchfile *wf;

	dlog( LOG_THREE, "stop(%p)", item );

	forTLIST( rp, n, item->processes, runprocess* ) {
		stop_process(rp);
	}
	forTLIST( lf, n, item->logfiles, logfile* ) {
		stop_logfile(lf);
	}
	forTLIST( wf, n, item->watchfiles, watchfile* ) {
		stop_watchfile(wf);
	}
	return false;
}

void free_inotify_watch( int *h )
{
	inotify_rm_watch( mon->inotify_fd, *h );
	mon->watchfiles->Del( *h );
}
bool Monitor::stop_watchfile( watchfile *lf )
{
	if( !lf->valid )
		return false;

	if( lf->watch->count > 0 ) {
		lf->watch->Clear( (voidFunction*)free_inotify_watch );
	}
	lf->valid = false;
	return false;
}

bool Monitor::stop_logfile( logfile *lf )
{
	if( !lf->valid )
		return false;
	if( lf->watchdesc >= 0 ) {
		inotify_rm_watch( inotify_fd, lf->watchdesc );
	}

	lf->valid = false;
	return false;
}

bool Monitor::stop_process( runprocess *rp )
{
	Lprintf(rp, "Attempt to stop process....\n");
	rp->runstate = MON_STATE_CRASHED;
	if( rp->stopcmd ) {
		Lprintf(rp, "Sending stop command %s\n", rp->stopcmd);
		QueueCommand( rp->stopcmd );
	} else if( rp->pid > 0 ) {
		Lprintf(rp, "Sending kill to pid %d.\n", rp->pid);
		kill(rp->pid, SIGTERM);
		sleep(2);
		kill(rp->pid, SIGKILL);
	}

	if( rp->shellproc ) {
		Lprintf(rp, "Closing pipe.\n");
		close_process_pipe( rp );
	}

	lprintf("Done.");
	return true;
}

void Monitor::Restart( void )
{
	tnode *n;
	monitor_item *item;

	forTLIST( item, n, items, monitor_item* ) {
		Restart(item);
	}
}

void Monitor::Restart( monitor_item *item )
{
	tnode *n;
	runprocess *rp;

	Lprintf(item, "Restarting group");
	forTLIST( rp, n, item->processes, runprocess* ) {
		restart_process(rp);
	}
}

bool Monitor::restart_process( runprocess *rp )
{
	monitor_item *item = rp->monitem;
	dlog( LOG_FOUR, "restart_process(%p, %p)", item, rp );

	// stop:
	if( rp->servicename ) {
		stringbuf *sb = new stringbuf();
		sb->printf("service %s stop\n", rp->servicename);
		QueueCommand(sb->p);
		delete sb;
	} else {
		stop_process(rp);
	}

	// start:
	rp->paused = false;
	rp->start_time = time(NULL);
	return start_process(item, rp);
}


