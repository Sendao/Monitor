#include "main.hh"

/*

void LoadRevisions( Monitor *m )
{
	stringbuf sb;
	sb.printf("%s%s.versions", *m->cfgfile == '.' ? "" : ".", m->cfgfile);

	char *filebuf = readFile( sb.p );
	tlist *lines = split(filebuf, "\n");
	tlist *words;
	tnode *n;
	char *line, *word, *word2;

	monitor_item *item = NULL;

	forTLIST( line, n, lines, char* ) {
		words = split(line, " ");
		word = (char*)words->FullPop();

		if( str_c_cmp(word, "item") == 0 ) {
			free(word);
			word = join(words, " ");

			item = (monitor_item*)mon->items0->Get( word );
			if( !item ) {
				lprintf("LoadRevisions: Cannot find group '%s'", word);
				free(word);
				words->Clear(free);
				delete words;
				continue;
			}
			if( !item->rev_full )
				item->rev_full = new tlist();
		}
		if( item && str_c_cmp(word, "branch") == 0 ) {

			revision *rev = new_revision();

			word2 = (char*)words->FullPop();
			rev->release_at = atol( word2 );
			free(word2);
			word2 = (char*)words->FullPop();
			rev->build_at = atol( word2 );
			free(word2);
			word2 = (char*)words->FullPop();
			rev->launch_at = atol( word2 );
			free(word2);
			rev->branch = join( words, " " );

			item->rev_full->PushBack(rev);
			if( !item->rev_current )
				item->rev_current = rev;

		}
		free(word);
		words->Clear(free);
		delete words;
	}
	lines->Clear(free);
	delete lines;
}

void SaveRevisions( Monitor *m )
{
	stringbuf sb;
	sb.printf("%s%s.versions", *m->cfgfile == '.' ? "" : ".", m->cfgfile);
	FILE *fp = fopen( sb.p, "w" );

	tnode *n, *n2;
	monitor_item *mi;
	repository *repo;
	repo_branch *branch;
	revision *rev;

	tlist *repolist = repos->ToList();

	forTLIST( repo, n, repolist, repository* ) {
		fprintf( fp, "repo %s\nuser %s\nkey %s\n", repo->uri, repo->keyuser, repo->keyfile );
		forTLIST( branch, n2, repo->branches, repo_branch* ) {
			fprintf( fp, "branch %s\n", branch->name );
			forTLIST( rev, n3, branch->revisions, revision* ) {
				fprintf( fp, "rev %ld %ld %ld %s\n", rev->release_at, rev->build_at, rev->launch_at, rev->branch );

			}
		}
	}

	forTLIST( mi, n, m->items, monitor_item* ) {
		if( mi->rev_current ) {
			branch = mi->rev_current->branch;
			repo = branch->repo;
			fprintf( fp, "item %s\nrepo %s\nbranch %s\nrev %s\n", mi->name, repo->uri, branch->name, rev->commit );
		}
	}

	fclose(fp);
}

repository *GetRepo( const char *uri, const char *keyuser, const char *keyfile )
{
	repository *repo;

	repo = repos->Get(uri);
	if( repo ) {
		return repo;
	}
	repo = new_repository();

	repo->uri = strdup(uri);
	repo->keyuser = strdup(keyuser);
	repo->keyfile = strdup(keyfile);
	repo->

	get_repo_data(repo);
}

void get_repo_data( repository *repo )
{
	//! new Curler
}

void process_repo_branches( repository *repo, const char *data )
{
}

*/
