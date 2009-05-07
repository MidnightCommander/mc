/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/* (C) 1998 Red Hat Software, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.redhat.com/pub/code/popt */

/** \file popthelp.c
 *  \brief Source: popt helper module
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "popt.h"
#include "poptint.h"

static void displayArgs(poptContext con, enum poptCallbackReason foo, 
			struct poptOption * key, 
			const char * arg, void * data)
{
    (void) foo;
    (void) arg;
    (void) data;

    if (key->shortName== '?')
	poptPrintHelp(con, stdout, 0);
    else
	poptPrintUsage(con, stdout, 0);
    exit(0);
}



static const char *
getTableTranslationDomain(const struct poptOption *table)
{
  const struct poptOption *opt;

  for(opt = table;
      opt->longName || opt->shortName || opt->arg.p;
      opt++) {
    if(opt->argInfo == POPT_ARG_INTL_DOMAIN)
      return opt->arg.p;
  }

  return NULL;
}

static const char * getArgDescrip(const struct poptOption * opt,
				  const char *translation_domain) {
    struct poptOption *poptHelpOptions;

    (void) translation_domain;

    poptHelpOptions = g_malloc(sizeof(struct poptOption)*4);

    poptHelpOptions[0].longName		= NULL;
    poptHelpOptions[0].shortName	= '\0';
    poptHelpOptions[0].argInfo		= POPT_ARG_CALLBACK;
    poptHelpOptions[0].arg.f1		= &displayArgs;
    poptHelpOptions[0].val		= '\0';
    poptHelpOptions[0].descrip		= NULL;
    poptHelpOptions[0].argDescrip	= NULL;

    poptHelpOptions[1].longName		= "help";
    poptHelpOptions[1].shortName	= '?';
    poptHelpOptions[1].argInfo		= 0;
    poptHelpOptions[1].arg.p		= NULL;
    poptHelpOptions[1].val		= '?';
    poptHelpOptions[1].descrip		= _("Show this help message");
    poptHelpOptions[1].argDescrip	= NULL;

    poptHelpOptions[2].longName		= "usage";
    poptHelpOptions[2].shortName	= '\0';
    poptHelpOptions[2].argInfo		= 0;
    poptHelpOptions[2].arg.p		= NULL;
    poptHelpOptions[2].val		= 'u';
    poptHelpOptions[2].descrip		= _("Display brief usage message");
    poptHelpOptions[2].argDescrip	= NULL;

    poptHelpOptions[3].longName		= NULL;
    poptHelpOptions[3].shortName	= '\0';
    poptHelpOptions[3].argInfo		= 0;
    poptHelpOptions[3].arg.p		= NULL;
    poptHelpOptions[3].val		= 0;
    poptHelpOptions[3].descrip		= NULL;
    poptHelpOptions[3].argDescrip	= NULL;

    if (!(opt->argInfo & POPT_ARG_MASK)){
	g_free(poptHelpOptions);
	return NULL;
    }

    if (opt == (poptHelpOptions + 1) || opt == (poptHelpOptions + 2))
	if (opt->argDescrip)
	{
	    g_free(poptHelpOptions);
	    return POPT_(opt->argDescrip);
	}

    if (opt->argDescrip)
    {
	g_free(poptHelpOptions);
	return D_(translation_domain, opt->argDescrip);
    }
    g_free(poptHelpOptions);
    return _("ARG");
}

static void singleOptionHelp(FILE * f, int maxLeftCol, 
			     const struct poptOption * opt,
			     const char *translation_domain) {
    int indentLength = maxLeftCol + 5;
    int lineLength = 79 - indentLength;
    const char * help = D_(translation_domain, opt->descrip);
    int helpLength;
    const char * ch;
    char * left;
    const char * argDescrip = getArgDescrip(opt, translation_domain);

    left = malloc(maxLeftCol + 1);
    *left = '\0';

    if (opt->longName && opt->shortName)
	sprintf(left, "-%c, --%s", opt->shortName, opt->longName);
    else if (opt->shortName) 
	sprintf(left, "-%c", opt->shortName);
    else if (opt->longName)
	sprintf(left, "--%s", opt->longName);
    if (!*left) return ;
    if (argDescrip) {
	strcat(left, "=");
	strcat(left, argDescrip);
    }

    if (help)
	fprintf(f,"  %-*s   ", maxLeftCol, left);
    else {
	fprintf(f,"  %s\n", left); 
	goto out;
    }

    helpLength = strlen(help);
    while (helpLength > lineLength) {
	ch = help + lineLength - 1;
	while (ch > help && !isspace((unsigned char) *ch)) ch--;
	if (ch == help) break;		/* give up */
	while (ch > (help + 1) && isspace((unsigned char) *ch)) ch--;
	ch++;

	fprintf(f, "%.*s\n%*s", (int) (ch - help), help, indentLength, " ");
	help = ch;
	while (isspace((unsigned char) *help) && *help) help++;
	helpLength = strlen(help);
    }

    if (helpLength) fprintf(f, "%s\n", help);

out:
    free(left);
}

static int maxArgWidth(const struct poptOption * opt,
		       const char * translation_domain) {
    int max = 0;
    int this;
    const char * s;
    
    while (opt->longName || opt->shortName || opt->arg.p) {
	if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) {
	    this = maxArgWidth(opt->arg.p, translation_domain);
	    if (this > max) max = this;
	} else if (!(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN)) {
	    this = opt->shortName ? 2 : 0;
	    if (opt->longName) {
		if (this) this += 2;
		this += strlen(opt->longName) + 2;
	    }

	    s = getArgDescrip(opt, translation_domain);
	    if (s)
		this += strlen(s) + 1;
	    if (this > max) max = this;
	}

	opt++;
    }
    
    return max;
}

static void singleTableHelp(FILE * f, const struct poptOption * table, 
			    int left,
			    const char *translation_domain) {
    const struct poptOption * opt;
    const char *sub_transdom;

    opt = table;
    while (opt->longName || opt->shortName || opt->arg.p) {
	if ((opt->longName || opt->shortName) && 
	    !(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN))
	    singleOptionHelp(f, left, opt, translation_domain);
	opt++;
    }

    opt = table;
    while (opt->longName || opt->shortName || opt->arg.p) {
	if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) {
	    sub_transdom = getTableTranslationDomain(opt->arg.p);
	    if(!sub_transdom)
		sub_transdom = translation_domain;
	    
	    if (opt->descrip)
		fprintf(f, "\n%s\n", D_(sub_transdom, opt->descrip));

	    singleTableHelp(f, opt->arg.p, left, sub_transdom);
	}
	opt++;
    }
}

static int showHelpIntro(poptContext con, FILE * f) {
    int len = 6;
    const char * fn;

    fprintf(f, _("Usage:"));
    if (!(con->flags & POPT_CONTEXT_KEEP_FIRST)) {
	fn = con->optionStack->argv[0];
	if (strrchr(fn, '/')) fn = strrchr(fn, '/') + 1;
	fprintf(f, " %s", fn);
	len += strlen(fn) + 1;
    }

    return len;
}

int poptPrintHelp(poptContext con, FILE * f, int flags) {
    int leftColWidth;

    (void) flags;

    showHelpIntro(con, f);
    if (con->otherHelp)
	fprintf(f, " %s\n", con->otherHelp);
    else
	fprintf(f, " %s\n", POPT_("[OPTION...]"));

    leftColWidth = maxArgWidth(con->options, NULL);
    singleTableHelp(f, con->options, leftColWidth, NULL);
    return leftColWidth;
}

static int singleOptionUsage(FILE * f, int cursor, 
			     const struct poptOption * opt,
			     const char *translation_domain) {
    int len = 3;
    char shortStr[2];
    const char * item = shortStr;
    const char * argDescrip = getArgDescrip(opt, translation_domain);

    if (opt->shortName) {
	if (!(opt->argInfo & POPT_ARG_MASK)) 
	    return cursor;	/* we did these already */
	len++;
	*shortStr = opt->shortName;
	shortStr[1] = '\0';
    } else if (opt->longName) {
	len += 1 + strlen(opt->longName);
	item = opt->longName;
    }

    if (len == 3) return cursor;

    if (argDescrip) 
	len += strlen(argDescrip) + 1;

    if ((cursor + len) > 79) {
	fprintf(f, "\n       ");
	cursor = 7;
    } 

    fprintf(f, " [-%s%s%s%s]", opt->shortName ? "" : "-", item,
	    argDescrip ? (opt->shortName ? " " : "=") : "",
	    argDescrip ? argDescrip : "");

    return cursor + len + 1;
}

static int singleTableUsage(FILE * f, int cursor, const struct poptOption * table,
		     const char *translation_domain) {
    const struct poptOption * opt;
    
    opt = table;
    while (opt->longName || opt->shortName || opt->arg.p) {
        if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INTL_DOMAIN)
	    translation_domain = (const char *)opt->arg.p;
	else if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE) 
	    cursor = singleTableUsage(f, cursor, opt->arg.p,
				      translation_domain);
	else if ((opt->longName || opt->shortName) && 
		 !(opt->argInfo & POPT_ARGFLAG_DOC_HIDDEN))
	  cursor = singleOptionUsage(f, cursor, opt, translation_domain);

	opt++;
    }

    return cursor;
}

static int showShortOptions(const struct poptOption * opt, FILE * f, 
			    char * str) {
    char s[300];		/* this is larger then the ascii set, so
				   it should do just fine */

    if (!str) {
	str = s;
	memset(str, 0, sizeof(s));
    }

    while (opt->longName || opt->shortName || opt->arg.p) {
	if (opt->shortName && !(opt->argInfo & POPT_ARG_MASK))
	    str[strlen(str)] = opt->shortName;
	else if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE)
	    showShortOptions(opt->arg.p, f, str);

	opt++;
    } 

    if (s != str || !*s)
	return 0;

    fprintf(f, " [-%s]", s);
    return strlen(s) + 4;
}

void poptPrintUsage(poptContext con, FILE * f, int flags) {
    int cursor;

    (void) flags;

    cursor = showHelpIntro(con, f);
    cursor += showShortOptions(con->options, f, NULL);
    singleTableUsage(f, cursor, con->options, NULL);

    if (con->otherHelp) {
	cursor += strlen(con->otherHelp) + 1;
	if (cursor > 79) fprintf(f, "\n       ");
	fprintf(f, " %s", con->otherHelp);
    }

    fprintf(f, "\n");
}

void poptSetOtherOptionHelp(poptContext con, const char * text) {
    if (con->otherHelp) free(con->otherHelp);
    con->otherHelp = strdup(text);
}
