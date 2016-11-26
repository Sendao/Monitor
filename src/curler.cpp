#include "main.hh"

CURLcode startup_code= CURLE_FAILED_INIT;

Curler::Curler()
{
	if( startup_code == CURLE_FAILED_INIT ) {
		startup_code = curl_global_init(0);
	}
	if( startup_code != 0 ) {
		lprintf("startup_code=%d", startup_code);
	}
	handle0 = curl_multi_init();

	handles0 = new DMap(1024);
	handles1 = new tlist();
	current = 0;
	curl_timeout = 0;

	new_handles = false;
}

Curler::~Curler()
{
	curl_multi_cleanup(handle0);
	curl_global_cleanup();
}

Curlpage *Curler::Get( const char *url, url_handler *cb )
{
	Curlpage *cp = new Curlpage(this, url);

	cp->handler = cb;
	handles1->PushBack(cp);
	long x = (long)cp->handle1;
	handles0->Set( x, cp );
	return cp;
}

int Curler::FD_RESET( fd_set *ins, fd_set *exs, struct timeval *timeout )
{
	int maxfd=0;
	fd_set outs;
	long prev_timeout = timeout->tv_sec*1000 + timeout->tv_usec/1000;

	if( curl_timeout <= 0 || new_handles ) {
		if( curl_multi_timeout( curl->handle0, &curl_timeout ) != 0 ) {
			maxfd = -1;
			curl_timeout = 1000;
			lprintf("Curl_multi_timeout error");
		} else {
			curl_multi_fdset(handle0, ins, &outs, exs, &maxfd);
		}
	} else {
		maxfd = -1;
	}

	if( curl_timeout < 0 ) curl_timeout = 500;

	if( curl_timeout < prev_timeout ) {
		timeout->tv_sec = curl_timeout / 1000;
		timeout->tv_usec = (curl_timeout%1000)*1000;
	}
	return maxfd;
}

void Curler::Process( void )
{
	int msgq=0;
	struct CURLMsg *m;
	Curlpage *cp;

	while( (m=curl_multi_info_read(handle0,&msgq)) ) {
		//lprintf("got curl_mh");
		if( m->msg == CURLMSG_DONE ) {
			// finished one
			CURL *e = m->easy_handle;
			// locate it
			long x = (long)e;
			cp = (Curlpage*)handles0->Get( x );
			// run callback
			if( cp->handler )
				cp->handler(this,cp);
			// clean up
			handles0->Del( x );
			tnode *n = handles1->Pull( cp );
			delete n;
			delete cp;
		}
	}
}

void Curler::FD_CHECK( fd_set *ins, fd_set *exs, long idle_cycles )
{
	int last = current;
	int rv;

	curl_timeout -= idle_cycles;
//	lprintf("timeout: %ld (idle: %ld)", curl_timeout, idle_cycles);
	if( curl_timeout > 0 ) return;

	//lprintf("curl fd_check");
	do {
		rv=curl_multi_perform( handle0, &current );
//		lprintf("current=%d, rv=%d", current, rv);
	} while( rv == CURLM_CALL_MULTI_PERFORM );

	if( current != last ) {
		Process();
	}
}

Curlpage::Curlpage(Curler *curler)
{
	c0 = curler;
	handle1 = curl_easy_init();
	contents = new stringbuf();
	headers = NULL;
	handler = NULL;
	url = NULL;
	userdata = NULL;
//	lprintf("curlpage:new blank");
}
Curlpage::Curlpage(Curler *curler, const char *_url)
{
	c0 = curler;
	handle1 = curl_easy_init();
	contents = new stringbuf();
	headers = NULL;
	handler = NULL;
	url = NULL;
	mydata = NULL;
	userdata = NULL;
//	lprintf("curlpage:new");

	seturl(_url);
}

Curlpage::~Curlpage()
{
	if( mydata ) free(mydata);
//	lprintf("curlpage:delete");
	curl_multi_remove_handle(c0->handle0, handle1);
	curl_easy_cleanup(handle1);
	if( headers ) curl_slist_free_all(headers);
	if( contents ) delete contents;
}

void Curlpage::seturl(const char *_url)
{
	if( url ) free(url);
	url = strdup(_url);
	curl_easy_setopt(handle1, CURLOPT_URL, url);
}
size_t curlpage_get( void *contents, size_t size, size_t n, void *p )
{
	//lprintf("curlpage_get");
	Curlpage *cp = (Curlpage*)p;
	char *x = (char*)malloc(size*n + 1);
	memcpy( x, contents, size*n );
	x[size*n] = 0;
	cp->contents->append(x);
	if( strstr( cp->contents->p, "\"response\" : \"ok\"" ) == NULL ) {
		lprintf("Curl got error:\n%s", x);
	}
	free(x);
	return size*n;
}
size_t curlpage_put( char *buffer, size_t size, size_t n, void *p )
{
	char *p_str = (char*)p;
	size_t len = strlen(p_str);
	//lprintf("curlpage_put(%s)", p_str);
	memcpy( buffer, p_str, len );
	buffer[len] = '\n';
	buffer[len+1] = 0;
	return len+1;
}
void Curlpage::attachdata( const char *data )
{
/*	curl_easy_setopt( handle1, CURLOPT_POST, 1L );
	curl_easy_setopt( handle1, CURLOPT_READDATA, strdup(data) );
	curl_easy_setopt( handle1, CURLOPT_READFUNCTION, curlpage_put );
	curl_easy_setopt( handle1, CURLOPT_INFILESIZE_LARGE, (curl_off_t)strlen(data) ); */
	mydata = strdup(data);
	mydatasize = strlen(data);
	//lprintf("curl:attach(%s)", data);
	curl_easy_setopt( handle1, CURLOPT_POSTFIELDSIZE, mydatasize );
	curl_easy_setopt( handle1, CURLOPT_POSTFIELDS, mydata );

}
void Curlpage::open(void)
{
	headers = curl_slist_append(headers, "content-type:text/plain");
	curl_easy_setopt(handle1, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handle1, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(handle1, CURLOPT_WRITEFUNCTION, curlpage_get);
	curl_easy_setopt(handle1, CURLOPT_WRITEDATA, this);

	//lprintf("curlpage:open (%s)", url);
	curl_multi_add_handle(c0->handle0, handle1);
	c0->new_handles = true;
}
