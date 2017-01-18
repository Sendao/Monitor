#include "main.hh"

// load config for monitor
//! todo: add 'post' command for reniceing process
//! eg post renice -n 19 $PID
//! todo: add 'watch' command for restarting dev
monitor_item *cfg_item=NULL;
char *mainlogfn;
logfile *cfg_lf=NULL;
watchfile *cfg_wf=NULL;
runprocess *cfg_rp=NULL;

void cfg_create_group( const char *value )
{
	cfg_item = new_monitor_item();
	cfg_item->m = mon;
	cfg_item->name = strdup(value);

	cfg_lf = NULL;
	cfg_rp = NULL;

	mon->items->PushBack(cfg_item);
	mon->items0->Set(value, cfg_item);
	lprintf("Added new group %s", cfg_item->name);
}
void cfg_require( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: require without group.");
		return;
	}

	cfg_item->requires->PushBack( strdup(value) );
//	forTLIST( item, n, mon->items, monitor_item* ) {
//		if( str_cmp( item->name, value ) == 0 ) {
}
void cfg_maxlogsize( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: maxlogsize without group.");
		return;
	}

	cfg_item->max_logfile_size = DiskSize(value);
}
void cfg_maxlogcount( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: maxlogcount without group.");
		return;
	}

	cfg_item->max_logfile_count = atol(value);
}
void cfg_trimlogsize( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: trimlogsize without group.");
		return;
	}

	cfg_item->trim_logfile_size = DiskSize(value);
}
void cfg_trimlogcount( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: trimlogcount without group.");
		return;
	}

	cfg_item->trim_logfile_count = atol(value);
}
void cfg_logarchivecmd( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: logarchivecmd without group.");
		return;
	}

	if( cfg_item->log_archive_cmd ) free(cfg_item->log_archive_cmd);
	cfg_item->log_archive_cmd = strdup(value);
}

void cfg_warmup( const char *value )
{
	if( !cfg_item || !cfg_rp ) {
		lprintf("Config: warmup without process.");
		return;
	}

	cfg_rp->warmup = atoi(value);
}

void cfg_logto( const char *value )
{
	if( !cfg_item ) {
		lprintf("Main logfile: %s", value);
		if( mainlogfn ) free(mainlogfn);
		mainlogfn = strdup(value);
		return;
	}

	if( cfg_rp ) {
		if( cfg_rp->logtofn ) free(cfg_rp->logtofn);
		cfg_rp->logtofn = strdup(value);
	} else {
		if( cfg_item->mainlog ) free(cfg_item->mainlog);
		cfg_item->mainlog = strdup(value);

	}
}

void cfg_loggly_key( const char *value )
{
	if( !cfg_item ) {
		if( mon->logglyKey ) free(mon->logglyKey);
		mon->logglyKey = strdup(value);
		return;
	}

	if( cfg_item->logglyKey ) free(cfg_item->logglyKey);
	cfg_item->logglyKey = strdup(value);
}
void cfg_loggly_tag( const char *value )
{
	if( !cfg_item ) {
		if( mon->logglyTag ) free(mon->logglyTag);
		mon->logglyTag = strdup(value);
		return;
	}

	if( cfg_item->logglyTag ) free(cfg_item->logglyTag);
	cfg_item->logglyTag = strdup(value);
}

void cfg_create_logfile( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: logfile without group.");
		return;
	}
	cfg_lf = new_logfile();
	cfg_lf->path = strdup(value);
	cfg_lf->monitem = cfg_item;
	cfg_item->logfiles->PushBack(cfg_lf);

	cfg_rp = NULL;
	lprintf("Added new logfile %s", cfg_lf->path);
}
void cfg_logfile_crashlines( const char *value )
{
	if( !cfg_lf ) {
		if( cfg_rp ) {
			cfg_rp->crashlines = atoi(value);
		}
		lprintf("Config: crashlines without logfile.");
		return;
	}
	cfg_lf->crashlines = atoi(value);
}



void cfg_create_watch( const char *value )
{
	tlist *values = split(value, " ");
	char *path = (char*)values->FullPop();
	char *matchX = (char*)values->FullPop();
	tlist *matches = split(matchX, ",");
	tnode *n;
	char *match;

	lprintf("cfg create watch: (%s): %s,%s(%d)",
			value, path, matchX, matches->count);
	free(matchX);

	watchfile *wf = new_watchfile();

	wf->path = path;
	wf->subdirs = true;
	forTLIST( match, n, matches, char* ) {
		wf->match->PushBack( match );
		lprintf("Adding watch pattern %s %s", path, match);
	}
	delete matches;
	values->Clear( free );
	delete values;
	cfg_item->watchfiles->PushBack(wf);
	cfg_wf = wf;
}

void cfg_subdirs( const char *value )
{
	//! todo: support logfiles
	if( !cfg_wf ) {
		lprintf("Config: subdirs without watch.");
		return;
	}

	cfg_wf->subdirs = BooleanString( value, true );
}

void cfg_action( const char *value )
{
	if( !cfg_wf ) {
		lprintf("Config: action without watch.");
		return;
	}

	// action: command to run on watchfile change.
	// default||restart: restart the group.
	if( cfg_wf->action ) free(cfg_wf->action);
	cfg_wf->action = strdup(value);
}


void cfg_create_process( const char *value )
{
	if( !cfg_item ) {
		lprintf("Config: process without group.");
		return;
	}
	cfg_rp = new_runprocess();
	cfg_rp->name = strdup(value);
	cfg_rp->monitem = cfg_item;
	cfg_rp->autostart = true;
	cfg_rp->keeprunning = true;
	if( cfg_item->mainlog )
		cfg_rp->logtofn = strdup( cfg_item->mainlog );
	cfg_item->processes->PushBack(cfg_rp);

	cfg_lf = NULL;
	lprintf("Added new process %s", cfg_rp->name);
}

void cfg_keeprunning( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: keeprunning without process.");
		return;
	}
	cfg_rp->keeprunning = BooleanString( value, true );
}

void cfg_psgrep( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: psgrep without process.");
		return;
	}
	if( cfg_rp->psgrep ) free(cfg_rp->psgrep);
	cfg_rp->psgrep = strdup(value);
}
void cfg_cwd( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: cwd without process.");
		return;
	}
	if( cfg_rp->cwd ) free(cfg_rp->cwd);
	cfg_rp->cwd = strdup(value);
}
void cfg_env( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: env without process.");
		return;
	}
	if( cfg_rp->env ) free(cfg_rp->env);
	cfg_rp->env = strdup(value);
}
void cfg_start( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: start without process.");
		return;
	}
	if( cfg_rp->startcmd ) free(cfg_rp->startcmd);
	cfg_rp->startcmd = strdup(value);
}
void cfg_stop( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: stop without process.");
		return;
	}
	if( cfg_rp->stopcmd ) free(cfg_rp->stopcmd);
	cfg_rp->stopcmd = strdup(value);
}
void cfg_service( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: service without process.");
		return;
	}
	if( cfg_rp->servicename ) free(cfg_rp->servicename);
	cfg_rp->servicename = strdup(value);
}
void cfg_autostart( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: autostart without process.");
		return;
	}

	cfg_rp->autostart = BooleanString( value, true );
}
void cfg_use_shellpid( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: shellpid without process.");
		return;
	}

	cfg_rp->use_shellpid = BooleanString( value, true );
}
void cfg_noshell( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: noshell without process.");
		return;
	}

	cfg_rp->noshell = BooleanString( value, false );
}
void cfg_newsid( const char *value )
{
	if( !cfg_rp ) {
		lprintf("Config: newsid without process.");
		return;
	}

	cfg_rp->use_newsid = BooleanString( value, true );
}

typedef void config_func( const char * );

void loadConfig( const char *path )
{
	char *filebuf = readFile(path);
	tlist *lines = split(filebuf, "\n");
	tlist *parsed = NULL;
	tnode *n;
	char *line, *key, *value;

	SMap *config_funcs = new SMap(32);

	mainlogfn = strdup("/tmp/monitor.log");


	config_funcs->Set( "group", (void*)cfg_create_group );
	config_funcs->Set( "warmup", (void*)cfg_warmup );
	config_funcs->Set( "require", (void*)cfg_require );

	config_funcs->Set( "maxlogsize", (void*)cfg_maxlogsize );
	config_funcs->Set( "maxlogcount", (void*)cfg_maxlogcount );
	config_funcs->Set( "trimlogsize", (void*)cfg_trimlogsize );
	config_funcs->Set( "trimlogcount", (void*)cfg_trimlogcount );
	config_funcs->Set( "logarchivecmd", (void*)cfg_logarchivecmd );

	config_funcs->Set( "logfile", (void*)cfg_create_logfile );
	config_funcs->Set( "logglykey", (void*)cfg_loggly_key );
	config_funcs->Set( "logglytag", (void*)cfg_loggly_tag );
	config_funcs->Set( "crashlines", (void*)cfg_logfile_crashlines );
	config_funcs->Set( "watch", (void*)cfg_create_watch );
	config_funcs->Set( "subdirs", (void*)cfg_subdirs );
	config_funcs->Set( "action", (void*)cfg_action );

	config_funcs->Set( "process", (void*)cfg_create_process );
	config_funcs->Set( "autostart", (void*)cfg_autostart );
	config_funcs->Set( "keeprunning", (void*)cfg_keeprunning );
	config_funcs->Set( "start", (void*)cfg_start );
	config_funcs->Set( "stop", (void*)cfg_stop );
	config_funcs->Set( "service", (void*)cfg_service );
	config_funcs->Set( "logto", (void*)cfg_logto );
	config_funcs->Set( "psgrep", (void*)cfg_psgrep );
	config_funcs->Set( "env", (void*)cfg_env );
	config_funcs->Set( "cwd", (void*)cfg_cwd );
	config_funcs->Set( "shellpid", (void*)cfg_use_shellpid );
	config_funcs->Set( "noshell", (void*)cfg_noshell );
	config_funcs->Set( "newsid", (void*)cfg_newsid );

	config_func *cb;

	forTLIST( line, n, lines, char* ) {
		if( *line == '#' ) // comment
			continue;

		parsed = split(line, ":");

		if( parsed->count <= 0 ) continue;

		if( parsed->count == 1 ) {

		} else {
			key = (char*)parsed->FullPop();
			strip_newline(key);
			value = join( parsed, ":" );
			strip_newline(value);
			//lprintf("Config '%s': '%s'", key, value);
			cb = (config_func*)config_funcs->Get(key);
			if( cb ) {
				cb(value);
			}
			free(value);
			free(key);
		}
	}

	lines->Clear( free );
	delete lines;
}
