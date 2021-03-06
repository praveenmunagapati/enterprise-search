/*
 * Eirik A. Nygaard
 * February 2007
 */

#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../common/bstr.h"
#include "../3pLibs/keyValueHash/hashtable.h"
#include "../common/ht.h"

#include "suggest.h"
#include "suffixtree.h"
#include "acl.h"

#define MIN_FREQ 15 


int
suggest_add_item(struct suggest_data *sd, char *word, int freq, char *aclallow, char *acldeny, struct hashtable *acls, char *collections)
{
	struct suggest_input *si;

	si = malloc(sizeof *si);
	if (si == NULL)
		return -1;

	si->word = strdup(word);
	if (si->word == NULL) {
		perror("strdup(word)");
		return -1;
	}
	si->frequency = freq;
#ifdef WITH_ACL
	si->aclallow = acl_parse_list(aclallow, acls);
	if (si->aclallow == NULL) {
		perror("acl_parse_list(aclallow)");
		return -1;
	}
	si->acldeny = acl_parse_list(acldeny, acls);
	if (si->acldeny == NULL) {
		perror("acl_parse_list(acldeny)");
		return -1;
	}
#endif
	si->collections = acl_parse_list(collections, acls);

	suffixtree_insert(&(sd->tree), si);

	return 0;
}


struct suggest_data *
suggest_init(void)
{
	struct suggest_data *sg;

	if ((sg = malloc(sizeof(*sg))) == NULL) {
		return NULL;
	}
	suffixtree_init(&(sg->tree));

	return sg;
}

void
suggest_destroy(struct suggest_data *sd)
{
	suffixtree_destroy(&(sd->tree));
	free(sd);
}

void
suggest_destroy_si(struct suggest_input *si)
{
#ifdef WITH_ACL
	if (si->aclallow)
		acl_destroy(si->aclallow);
	if (si->acldeny)
		acl_destroy(si->acldeny);
#endif
	free(si->word);
	free(si);
}

int
suggest_read_frequency(struct suggest_data *sd, char *wordlist)
{
	FILE *fp;
	int freq, linenum;
	int got;
	char word[1024];
	char aclallow[1024*1024];
	char acldeny[1024*1024];
	char collections[1024*1024];
	struct hashtable *acls;

	acls = create_hashtable(101, ht_stringhash, ht_stringcmp);

	if ((fp = fopen(wordlist, "r")) == NULL)
		return -1;

	linenum = 1;
	got = 0;
	while (!feof(fp)) {
		int ret;
		size_t len;
		char *line = NULL;
		char **linedata;
		acldeny[0] = '\0';

		if (getline(&line, &len, fp) == -1) {
			if (line != NULL)
				free(line);
			continue;
		}
		if (line == NULL || line[0] == '\0') {
			free(line);
			continue;
		}

		line[strlen(line)-1] = '\0'; // Remove newline

		if (split(line, " ", &linedata) < 2) {
			printf("'%s' %d\n", line, len);
			fprintf(stderr, "Unable to read parse line %d in %s\n", linenum, wordlist);
			linenum++;
			free(line);
			continue;
		}
		free(line);
		strscpy(word, linedata[0],sizeof(word));
		freq = atol(linedata[1]);
#ifdef WITH_ACL
		if (linedata[2])
			strscpy(aclallow, linedata[2],sizeof(aclallow));
		else
#endif
			strcpy(aclallow, "");
#ifdef WITH_ACL
		if (linedata[2] && linedata[3])
			strscpy(acldeny, linedata[3],sizeof(acldeny));
		else
#endif
			strcpy(acldeny, "");

		if (linedata[2] && linedata[3] && linedata[4])
			strscpy(collections, linedata[4], sizeof(collections));
		else
			strcpy(collections, "");
		
		/* Free linedata */
		{
			int ifree;

			for(ifree = 0; linedata[ifree] != NULL; ifree++)
				free(linedata[ifree]);
			free(linedata);
		}

		linenum++;
		if (freq < MIN_FREQ)
			continue;
		//printf("%s <=> %d\nGot acl allow: %s\nGot acl deny: %s\n", word, freq, aclallow, acldeny);
		if (suggest_add_item(sd, word, freq, aclallow, acldeny, acls, collections) == -1) {
			perror("suggest_add_item()");
		}

		got++;
		if ((linenum % 100000) == 0) {
			printf("100000 new word!\n");
		}
	}

	printf("Collected: %d\n", got);
	fclose(fp);

	return 0;
}

void
suggest_most_used(struct suggest_data *sd)
{
	printf("Gathering most used...\n");
	suffixtree_most_used(&sd->tree);
	printf("Done collecting.\n");
}

struct suggest_input **
#ifdef WITH_ACL
suggest_find_prefix(struct suggest_data *sd, char *prefix, char *user, char ***groups, int *num, char *collectionFilter)
#else
suggest_find_prefix(struct suggest_data *sd, char *prefix, char *collectionFilter)
#endif
{
#ifndef WITH_ACL
	char *user = NULL;
	char ***groups = NULL;
	int *num = NULL;
#endif 
	return suffixtree_find_prefix(&sd->tree, prefix, user, groups, num, collectionFilter);
}


#ifdef TEST_SUGGEST

int
main(int argc, char **argv)
{
	struct suggest_data *sd = suggest_init();
	struct suggest_input **si;
	char *word = "logge"; //"heimdalsmunnen";
	FILE *fp;
	int i;

	if (sd == NULL) {
		fprintf(stderr, "Could not initialize suggest\n");
		return 1;
	}
	//printf("%d\n", suggest_read_frequency(sd, "/home/eirik/Boitho/boitho/websearch/lot/1/1/Exchangetest/dictionarywords"));
	//printf("%d\n", suggest_read_frequency(sd, "/home/sdtest/src/boitho/websearch/var/somewords"));
	printf("%d\n", suggest_read_frequency(sd, "/home/sdtest/src/boitho/websearch/var/dictionarywords"));
	//printf("%d\n", suggest_read_frequency(sd, "testinput.list"));
	//printf("%d\n", suggest_read_frequency(sd, "liten.list"));
	//printf("%d\n", suggest_read_frequency(sd, "wordlist.7"));
#if 0
	suffixtree_most_used(&(sd->tree), 5);

	printf("Looking for word: %s\n", word);
	for (i = 0; i < 100; i++)
		suffixtree_find_word(&(sd->tree), word);
#endif

	//printf("Word: %p\n", suffixtree_find_word(&(sd->tree), "typeface"));
	//printf("Word: %p\n", suffixtree_find_word(&(sd->tree), "laure"));
	//printf("Word: %p\n", suffixtree_find_word(&(sd->tree), "nezu"));
	//
	//printf("Word: %x\n", suffixtree_find_word(&(sd->tree), "individualsiaiaiai"));
	//printf("Word: %x\n", suffixtree_find_word(&(sd->tree), "individ"));
	//
	suggest_most_used(sd);
	//si = suggest_find_prefix(sd, "individ");
	
	//printf("si... %x\n", *si);
#if 0
	printf("Printing the most used word starting with 'individ'\n");
	for (; *si != NULL; si++) {
		printf("Something: %s / %d\n", (*si)->word, (*si)->frequency);
	}
#endif
	
#if 0
	{
		char s[1024];
		int foo;
		struct suggest_input *si;
		printf("doing shit\n");
		fp = fopen("../../meta/UnikeTermerMedForekomst.ENG", "r");
		while (!feof(fp)) {
			if (fscanf(fp, "%s %d\n", s, &foo) != 2)
				continue;
			si = suffixtree_find_word(&(sd->tree), s);
			if (si == NULL) {
				printf("Could not find %s\n", s);
			}
		}
		fclose(fp);
	}
#endif

#if 0

	//printf("Count %d\n", suffixtree_scan(&(sd->tree), 0));
	{
		struct suffixtree *sf = &(sd->tree);
		int a;

		printf("The most used words in the dictionary:\n");
		for (a=0; sf->best[a] != NULL; a++) {
			printf("%s <=> %d\n", sf->best[a]->word, sf->best[a]->frequency);
		}
	}

	do {
		char *buf;
		size_t len;
		unsigned int size = 1024;
		struct suggest_input *si2;

		buf = NULL;
		printf("Enter prefix: ");
		fflush(stdin);
		len = getline(&buf, &size, stdin);
		buf[len-1] = '\0';
#ifdef WITH_ACL
		si = suggest_find_prefix(sd, buf, "eirik");
#else
		si = suggest_find_prefix(sd, buf);
#endif
		if (si == NULL) {
			printf("No match for '%s'\n", buf);
			free(buf);
			continue;
		}
		else {
			printf("Printing the most used word starting with '%s'\n", buf);
			for (; *si != NULL; si++) {
				printf("Something: %s / %d\n", (*si)->word, (*si)->frequency);
			}
		}
		free(buf);
	} while(1);
#endif

	time_t  t0, t1; /* time_t is defined on <time.h> and <sys/types.h> as long */
	clock_t c0, c1; /* clock_t is defined on <time.h> and <sys/types.h> as int */

	printf ("using UNIX function time to measure wallclock time ... \n");
	printf ("using UNIX function clock to measure CPU time ... \n");

	t0 = time(NULL);
	c0 = clock();

	printf ("\tbegin (wall):            %ld\n", (long) t0);
	printf ("\tbegin (CPU):             %d\n", (int) c0);

	char *foo = strdup("a");
	for (int i = 0; i < 1000; i++) {
		suggest_find_prefix(sd, foo, "en");
		foo[0]++;
		if (foo[0] > 'z')
			foo[0] = 'a';
	}

	t1 = time(NULL);
	c1 = clock();

	printf ("\tend (wall):              %ld\n", (long) t1);
	printf ("\tend (CPU);               %d\n", (int) c1);
	printf ("\telapsed wall clock time: %ld\n", (long) (t1 - t0));
	printf ("\telapsed CPU time:        %f\n", (float) (c1 - c0)/CLOCKS_PER_SEC);

	return 0;
}

#endif
