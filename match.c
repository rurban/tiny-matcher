/*
 * A tiny regular-expression pattern matcher.
 * Dynamic (no compilation), bounded recursive.
 * This variant uses heap memory and the string stdlib.
 *
 * There should be variants for embedded without malloc
 * and the string lib.
 *
 * Written by Reini Urban 2003.
 * MIT License
 *
 * Influenced by code by Steffen Offermann 1991 (public domain)
 * http://www.cs.umu.se/~isak/Snippets/xstrcmp.c
 */

// strsep
#define _DEFAULT_SOURCE
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#if defined _UNICODE
  #include <locale.h>
#elseif defined _UTF8
  #include "utf8.h"
#endif

#if defined DEBUG || defined TEST
#include <stdio.h>
#endif

/* this is just for standalone testing */
#ifdef DEBUG
#include <stdarg.h>
#define LOG_WARNING 3
#define LOG_DEBUG   0

void dbg_log(int level, const char *fmt, ...) {
    va_list ap;
    (void)level;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fflush(stdout);
}
#else
#define dbg_log(...)
#endif
/* end of standalone test functions */

int  patmatch_groupcounter = 0;
char patmatch_group[80] = "";

static int patmatch_repeated(const char *pattern, char *data,
			     const int num);

int patmatch(const char *pattern, char *data) 
{
    int i, border=0;
    char *where;
    static char prev = '\0';
    static char groupdata[80] = "";
    static char *group = patmatch_group;
#ifdef AST_PBX_MAX_STACK
    int groupcounter = patmatch_groupcounter;
#endif

    dbg_log(LOG_DEBUG, " >>> \"%s\" =~ /%s/\n", data, pattern);
    switch (*pattern)
	{
	case '\0':
	    dbg_log(LOG_DEBUG, " !>>> \"%s\" => %s\n", data, !*data ? "OK" : "FAIL");
	    return !*data;
	    
	//case ' ':
	//case '-':
	    /* Ignore these characters in the pattern */
	    //return *data && patmatch(pattern+1, data);

	case '*' : /* Match any sequence of characters. 0 or more. */
	    prev = *pattern;
	    if (! *(pattern+1) ) 
		return 1; /* return *data; => match one or more */
	    else if (*(pattern+1) == '?') // TODO non-greedy
		return patmatch(pattern+2, data) || (*data && patmatch(pattern, data+1));
	    else
		return patmatch(pattern+1, data) || (*data && patmatch(pattern, data+1));
	case '+' : /* Match 1 or more, greedy or non-greedy. */
	    prev = *pattern;
	    if (! *(pattern+1) ) 
		return *data;
	    else if (*(pattern+1) == '?') // TODO non-greedy
		return patmatch(pattern+2, data) || (*data && patmatch(pattern, data+1));
	    else
		return patmatch(pattern+1, data) || (*data && patmatch(pattern, data+1));

	/* wildcard character: Match any char */
	case '.' :
	    prev = *pattern;
	    return *data && patmatch(pattern+1, data+1);

	case '\\': 
	    prev = *pattern;
	    switch (*(pattern+1)) {
	    case 'd': /* 0-9 */
		return isdigit(*data) &&
		    patmatch(pattern+2, data+1);
	    case 'D': /* ^0-9 */
		return !isdigit(*data) &&
		    patmatch(pattern+2, data+1);
	    case 's': /* whitespace */
		return isspace(*data) &&
		    patmatch(pattern+2, data+1);
	    case 'S': /* ^whitespace */
		return !isspace(*data) &&
		    patmatch(pattern+2, data+1);
	    case 'w': /* word */
		return isalnum(*data) &&
		    patmatch(pattern+2, data+1);
	    case 'W': /* ^word */
		return !isalnum(*data) &&
		    patmatch(pattern+2, data+1);
	    case 'x': /* ^word */
		return !isalnum(*data) &&
		    patmatch(pattern+2, data+1);
	    default:
		return *data == '\\' &&
		    patmatch(pattern+2, data+1);
	    }
	    break;
	    
	case '{': /* quantifier {n[,m]} */
	    {
		char *comma;
		int cpos;
		where=strchr(pattern,'}');
		if (where) {
		    border=(int)(where-pattern);
		    comma = strchr(pattern,',');
		}
		if (!where || border > (int)strlen(pattern)) {
		    dbg_log(LOG_WARNING, "Wrong %s pattern usage\n", pattern);
		    return 0;
		} else {
		    char tmp[8];
		    int from, to;
		    if (comma)
			cpos = (int)(comma-pattern);
		    else 
			cpos = border;
		    strncpy(tmp,pattern+1,cpos-1);
		    tmp[cpos-1] = '\0';
		    from = atoi(tmp);
		    if (comma) {
			if (border-cpos > 1) { /* {f,t} */
			    strncpy(tmp,comma+1,border-cpos);
			    tmp[border-cpos+1] = '\0';
			    to = atoi(tmp);
			} else { /* {f,} */
			    to = strlen(data) + 1; /* may fail if after the group are more pattern chars */
			    if (*(pattern+border+1)) {
				to = to - strlen(pattern+border+1) + 1;
			    }
			}
		    } else {     /* {f} */
			if (from == 0) {
			    dbg_log(LOG_WARNING, "Invalid {0} pattern quantifier %s\n", pattern);
			    return 0;
			}
			to = from;
		    }
		    if (from < 0 || to <= 0 || to < from) {
			dbg_log(LOG_WARNING, "Invalid pattern quantifier %s\n", pattern);
			return 0;
		    }

		    if (*group) { 	/* check for repeated pattern{n,m} in previous group */
			int i;
			for (i=0; i< (int)strlen(group); i++) {
			    data--;
			}
			dbg_log(LOG_DEBUG, ">>> check for repeated pattern{%d,%d} of group '%s' in data '%s'\n", from, to, group, data);
			strcat(group,"+");
		    } else {
			dbg_log(LOG_DEBUG, ">>> check for repeated pattern{%d,%d} in previous character '%c'\n", from, to, prev);
			data--;
			group[0] = prev;
			group[1] = '+';
			group[2] = '\0';
		    }
		    *tmp = prev;
                    // this is the longest match. for shortest match switch direction
		    for (i=to; i>=from; i--) {
			if (patmatch_repeated(group,data,i))
                            break;
		    }
		    prev = *tmp;
		    if (i >= from || !from) { /* if found */
			dbg_log(LOG_DEBUG, " >>>> found '%s' in data '%s' after %d runs\n", group, data, i);
			char name[16];
			data += (i * (strlen(group) - 1) - 1);
			int l = strlen(groupdata) - strlen(data);
			/* data = data-i+from-1; */		/* possible failure here! */
			if (prev == ')') {			/* grouping => capture */
			    *(group+strlen(group)-1) = '\0';
			    groupdata[l+1] = '\0';
			    dbg_log(LOG_DEBUG, "  >>>>> end of group '%s', data: %s\n", group, groupdata);
			    /* capture the found data in variables $1, $2, ... */
#ifdef AST_PBX_MAX_STACK
			    sprintf(name,"%d",++groupcounter);
			    pbx_builtin_setvar_helper(NULL,name,groupdata);
#endif
			    dbg_log(LOG_DEBUG, "  >>>>> global variable $%s set to '%s'\n", name, groupdata);
			}
		    }
		    *group = '\0';
		    prev = '\0';
		    if (i >= from) {        /* found: continue */
			dbg_log(LOG_DEBUG, " >>>> found in round %d from %d\n", i, to);
			if (*data) {
			    if (*(pattern+border+1)) /* if the tail check fails, try the other rounds */
				if (patmatch(pattern+border+1, data+1))
				    return 1;
				else return (patmatch_repeated(group, data, i--) && 
					     patmatch(pattern+border+1, data+i));
			    else
				return patmatch(pattern+border+1, data+1);
			}
			else 
			    return 1;
		    } else if (from == 0) { /* not found, but special case from=0: no match needed */
			dbg_log(LOG_DEBUG, " >>>> not found, but no match needed and data exhausted\n");
			if (*data)
			    return patmatch(pattern+border+1, data+1);
			else 
			    return 1;
		    } else                /* not found */
			return 0;
		}
	    }
	    /* unreachable code */
	    
	case '(': /* grouping */
	    prev = *pattern;
	    if (*group) {
		dbg_log(LOG_WARNING, "Unexpected subgroup ( in pattern %s\n", pattern);
		return 0;
	    }
	    where=strchr(pattern,')');
	    if (where)
		border=(int)(where-pattern);
	    if (!where || border > (int)strlen(pattern)) {
		dbg_log(LOG_WARNING, "Wrong (%s) pattern usage\n", pattern);
		return 0;
	    }
	    strncpy(group,pattern+1,border-1);
	    group[border-1] = '\0';
	    strcpy(groupdata,data);
	    dbg_log(LOG_DEBUG, ">>> group '%s' stored, data: '%s'\n", group, groupdata);
	    if (strchr(pattern,'|')) { /* alternations */
		char *s, *scopy, *sep, *sepcopy;
		s = scopy = (char *) malloc(strlen(pattern));
		sepcopy   = (char *) malloc(strlen(pattern));
		strcpy(s,group);
		while ((sep = strsep(&s,"|"))) {
		    strcpy(sepcopy,sep);
		    strcat(sepcopy,pattern+border+1);
		    dbg_log(LOG_DEBUG, "  >>>> alternative '%s' =~ /%s/\n", sepcopy, data);
		    if (patmatch(sepcopy, data))
                        break;
		    if (!*data) { 
			sep = NULL; break; 
		    }
		}
		free(scopy);
		if (sep) { /* found */
		    free(sepcopy);
		    return 1;
		} else {
		    free(sepcopy);
		    return 0;
		}
	    } else {
		return patmatch(pattern+1, data);
	    }

	case ')': /* group end */
	    prev = *pattern;
	    if (!*group) {
		dbg_log(LOG_WARNING, "Unexpected ) in pattern %s\n", pattern);
		return 0;
	    } else {
		if (pattern[1] != '{') { /* capture without quantifiers */
		    char name[16];
		    int l = (int)(strlen(groupdata) - strlen(data));
		    groupdata[l-1] = '\0';
		    *(group+strlen(group)-1) = '\0';
		    dbg_log(LOG_DEBUG, ">>> end of group '%s', data: %s\n", group, groupdata);
		    /* capture the found data in variables $1, $2, ... */
#ifdef AST_PBX_MAX_STACK
		    sprintf(name,"%d",++groupcounter);
		    pbx_builtin_setvar_helper(NULL,name,groupdata);
#endif
		    dbg_log(LOG_DEBUG, ">>> global variable $%s set to '%s'\n", name, groupdata);
		    *group = '\0';
		}
	    }
	    return patmatch(pattern+1, data);

	case '|': /* alternation */
	    if (!*group) {
		dbg_log(LOG_WARNING, "Need group for | in %s\n", pattern);
		return 0;
	    }
#if 0
	    {
		char *s, *sep;
		s = strdup(group);
		while (sep = strsep(&s,"|")) {
		    dbg_log(LOG_DEBUG, "  >>>> alternative %d\n", sep);
		    if (patmatch(sep, data))
                        break;
		    if (!*data) { 
			*sep = '\0'; break; 
		    }
		}
		if (*sep) { /* found */
		    free(s);
		    return patmatch(pattern+1, data+1);
		} else {
		    free(s);
		    return 0;
		}
	    }
#endif
            break;

	case '[': /* Character ranges: [0-9a-zA-Z] */
	    prev = *pattern;
	    pattern++;
	    where=strchr(pattern,']');
	    if (where)
		border=(int)(where-pattern);
	    if (!where || border > (int)strlen(pattern)) {
		dbg_log(LOG_WARNING, "Wrong [%s] pattern usage\n", pattern);
		return 0;
	    }
	    if (*pattern == '^') { /* Negation like [^...] */
		for (i=1; i<border; i++) {
		    if (*data==pattern[i])
			return 0;
		    else if ((pattern[i+1]=='-') && (i+2<border)) {
			if (*data >= pattern[i] && *data <= pattern[i+2]) {
			    return 0;
			} else {
			    i+=2;
			    continue;
			}
		    }
		}
		return patmatch(where+1, data+1);
	    } else {
		for (i=0; i<border; i++) {
		    if (i+2<border) {
			if (*data==pattern[i])
			    return patmatch(where+1, data+1);
			else if (pattern[i+1]=='-') {
			    if (*data >= pattern[i] && *data <= pattern[i+2]) {
				return patmatch(where+1, data+1);
			    } else {
				i+=2;
				continue;
			    }
			}
		    }
		}
	    }
	    break;
	    
	default:
	    prev = *pattern;
	    return *pattern == *data && patmatch(pattern+1, data+1);
	}
    return 0;
}

/* try exactly num repetitions, from high to from */
static int patmatch_repeated(const char *pattern, char *data, const int num) 
{
    int i;
    dbg_log(LOG_DEBUG, "  >>> try %d repetitions of '%s' in data '%s'\n", num, pattern, data);
    if (num <= 0)
        return 0;
    for (i=1; i<=num; i++) {
	dbg_log(LOG_DEBUG, "  >>>> round %d with data %s\n", i, data);
	if (!patmatch(pattern, data))
            return 0;
	data = data + strlen(pattern) - 1;
    }
    return 1;
}

int match(char *pattern, char *data)
{
    int match;
    patmatch_groupcounter = 0;
    *patmatch_group = '\0';
    if (!*pattern) {
	dbg_log(LOG_WARNING, "match: empty pattern\n");
	return 0;
    }
    if (!*data) {
	dbg_log(LOG_WARNING, "match: empty data\n");
	return 0;
    }
    match = patmatch(pattern,data);
    dbg_log(LOG_DEBUG, "match %s =~ /%s/ => %d\n", data, pattern, match);
    return match;
}

#ifdef TEST

int testmatch(char *pattern, char *data, int should)
{
    int ret;

    ret = match(pattern, data);
    if (should == ret) {
	printf("OK\n");
    } else {
	printf("FAIL\n");
	exit(-1);
    }
    return ret;
}

int main () {
    char data1[] = "0316890002";
    char data2[] = "03168900028500";

    /* plain strcmp */
    testmatch("0316890002",data1,1);
    /* simple regex with ending . */
    testmatch("0\\d+",data1,1);
    /* not terminating . */
    testmatch("0\\d+0",data1,0);
#if 1
    testmatch("0\\d+8500",data1,0);
    testmatch("0\\d+8500",data2,1);
    testmatch("0[2-9]+0+8500",data2,1);
    /* ranges */
    testmatch("[a-z]o[0-9a-z]+","voicemail",1);
    testmatch("[0]o[0-9a-z]+","voicemail",0);

    /* negation */
    testmatch("[^0-9]o+","voicemail",1);
    testmatch("[^x]o+","voicemail",1);
    testmatch("[^v]o+","voicemail",0);
    testmatch("[^a-z]o+","voicemail",0);
#endif
    /* quantifiers */
    testmatch("0316890{2}\\d","0316890002",0);
    testmatch("0316890{3}\\d","0316890002",1);
    testmatch("0316890{1,}\\d","0316890002",1);
    testmatch("0316890{1,3}\\d","0316890002",1);
    testmatch("0316890{4,5}\\d","0316890002",0);
    /* grouping and capturing */
    testmatch("031689(0\\d\\d)\\d","0316890002",1);
    testmatch("031689(0\\d9)\\d","0316890002",0);
    testmatch("031689(\\d){1,3}\\d","0316890002",1);
    testmatch("031689(\\d){4,3}\\d","0316890002",0);
    testmatch("031689(\\d){3}\\d","0316890002",1);
    testmatch("031689(\\d){4}","0316890002",1);
    testmatch("031689(\\d){5}","0316890002",0);
    testmatch("031689(0){3}\\d","0316890002",1);
    testmatch("031689(\\d){4}\\d","0316890002",0);
    testmatch("031689(\\d){4}\\d","03168900021",0);
    testmatch("031689(\\d){4}[1-9]","03168900021",1);
    testmatch("031689(\\d\\d){2}[1-9]","03168900021",1);
    /* alternation */
    testmatch("(032|02)\\d.","0316890002",0);
    testmatch("(031\\d|0)\\d.","0316890002",1);
}

#endif
