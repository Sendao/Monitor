#ifndef __CURLER_H
#define __CURLER_H

typedef class Curler Curler;
typedef class Curlpage Curlpage;
typedef void url_handler( Curler *, Curlpage * );

class Curler
{
public:
	Curler();
	~Curler();

#ifdef _USE_CURL
	CURLM *handle0;
#else
	int *handle0;
#endif

	int current;

	DMap *handles0;
	tlist *handles1;
	long curl_timeout;
	bool new_handles;

public:
	int FD_RESET( fd_set *ins, fd_set *excepts, struct timeval *timeout );
	void FD_CHECK( fd_set *ins, fd_set *excepts, long idle_cycles );
	void Process(void);
	Curlpage *Get( const char *url, url_handler *cb );
};

class Curlpage
{
public:
	Curlpage(Curler*);
	Curlpage(Curler*, const char *_url);
	~Curlpage();

public:
	void seturl(const char *_url);
	void attachdata(const char *data);
	void open(void);

public:
	Curler *c0;
	char *url;
#ifdef _USE_CURL
	curl_slist *headers;
	CURL *handle1;
#else
	void *headers;
	void *handle1;
#endif
	url_handler *handler;
	char *mydata;
	size_t mydatasize;
	void *userdata;

public:
	stringbuf *contents;

};

size_t curlpage_get( void *contents, size_t size, size_t n, void *p );
size_t curlpage_put( char *buffer, size_t size, size_t n, void *p );


#endif
