#include "main.hh"

Curler::Curler()
{
}

Curler::~Curler()
{
}

Curlpage *Curler::Get( const char *url, url_handler *cb )
{
	return NULL;
}

int Curler::FD_RESET( fd_set *ins, fd_set *exs, struct timeval *timeout )
{
	return 0;
}

void Curler::Process( void )
{
}

void Curler::FD_CHECK( fd_set *ins, fd_set *exs, long idle_cycles )
{
}

Curlpage::Curlpage(Curler *curler)
{
}
Curlpage::Curlpage(Curler *curler, const char *_url)
{
}

Curlpage::~Curlpage()
{
}

void Curlpage::seturl(const char *_url)
{
}
size_t curlpage_get( void *contents, size_t size, size_t n, void *p )
{
	return 0;
}
size_t curlpage_put( char *buffer, size_t size, size_t n, void *p )
{
	buffer[0] = '\0';
	return 0;
}
void Curlpage::attachdata( const char *data )
{
	return;
}
void Curlpage::open(void)
{
	return;
}
