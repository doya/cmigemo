#include <stdio.h>
#include <time.h>

#include "wordbuf.h"
#include "wordlist.h"
#include "mnode.h"
#include "rxgen.h"
#include "migemo.h"

/*
 * main
 */

    int
query_loop(migemo* p)
{
    while (!feof(stdin))
    {
	unsigned char buf[256], *ans;
	printf("QUERY: ");
	if (!gets(buf))
	    break;
	ans = migemo_query(p, buf);
	if (ans)
	    printf("PATTERN: %s\n", ans);
	migemo_release(p, ans);
    }
    return 0;
}

    int
main(int argc, char** argv)
{
    if (1)
    {
	migemo *pmigemo;

	pmigemo = migemo_open("migemo-dict");
	if (!pmigemo)
	    pmigemo = migemo_open("../migemo-dict");
	if (pmigemo)
	{
#ifndef _PROFILE
	    printf("clock()=%d\n", clock());
	    query_loop(pmigemo);
#else
	    unsigned char *ans;
	    FILE* fp = fopen("exe.log", "wt");

	    ans = migemo_query(pmigemo, "a");
	    if (ans)
		fprintf(fp, "  [%s]\n", ans);
	    ans = migemo_query(pmigemo, "k");
	    if (ans)
		fprintf(fp, "  [%s]\n", ans);
	    fclose(fp);
#endif
	    migemo_close(pmigemo);
	}

#ifdef FULL_LINK_TEST
	/* 統計データ表示 */
	fprintf(stderr, "n_mnode_new=      %8d\n", n_mnode_new);
	fprintf(stderr, "n_mnode_delete=   %8d\n", n_mnode_delete);
	fprintf(stderr, "n_rnode_new=      %8d\n", n_rnode_new);
	fprintf(stderr, "n_rnode_delete=   %8d\n", n_rnode_delete);
	fprintf(stderr, "n_wordbuf_open=   %8d\n", n_wordbuf_open);
	fprintf(stderr, "n_wordbuf_close=  %8d\n", n_wordbuf_close);
	fprintf(stderr, "n_wordlist_new=   %8d\n", n_wordlist_new);
	fprintf(stderr, "n_wordlist_delete=%8d\n", n_wordlist_delete);
	fprintf(stderr, "n_wordlist_total= %8d\n", n_wordlist_total);
#endif
    }

    return 0;
}
