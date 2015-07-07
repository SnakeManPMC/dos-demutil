/*	procutil.c

	Common support routines for Falcon4 procppp / procae
	(C)opyright 2000 PMC TFW
	All rights reserved.

	notes:
	
	Used by both procae.c and procppp.c - if you change something here,
	both will be affected. You've been warned.
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <math.h>

#include <string.h>

#define DELIMITER 1
#define STRING 2
#define NUMBER 3

unsigned char *p;
char token[80];
char tok_type;

extern int argc;
extern char **argv;

int isdelim(char c)
{
	if(c==32 || c==9 || c=='\n' || c==0 || c=='\r')
		return 1;
	return 0;
}

int Check_Param (char *parm)
{
	int		i;

	for (i=1 ; i<argc ; i++) {
		if (!argv[i])
			continue;	
		if (!strcmp (parm,argv[i]))
			return i;
	}

	return 0;
}

void *Get_Next_Name(void)
{
	// fixme: stupid,replace with getoptions when you feel like it
	
	int i;
	static int current=1;

	for (i=current;i<argc;i++) {
		if(!argv[i])
			continue;
		if (strncmp("-",argv[i],1)) {
			current=i+1;
			return argv[i]; 
		}
	}
	return NULL;	
}

void *Get_Next_Option(void)
{
	// fixme: stupid,replace with getoptions when you feel like it
	
	int i;
	static int current=1;

	for (i=current;i<argc;i++) {
		if(!argv[i])
			continue;
		if (strncmp("-",argv[i],1)==0) {
			current=i+1;
			return argv[i]; 
		}
	}
	return NULL;	
}

void get_token(void)
{
	register char *temp;
	
	tok_type=0;
	temp=token;
	*temp='\0';
	
	if(!*p) return;
	
	while(isspace(*p)) ++p;
	
	if(isalpha(*p)) {
		while(!isdelim(*p)) *temp++=*p++;
		tok_type=STRING;
	} else if(isdigit(*p) || *p=='-') {
		while(!isdelim(*p)) *temp++=*p++;
		tok_type= NUMBER;
	}

	*temp='\0';	

}

char *rmallws(char *str)
{
      char *obuf, *nbuf;

      if (str)
      {
            for (obuf = str, nbuf = str; *obuf; ++obuf)
            {
                  if (!isspace(*obuf))
                        *nbuf++ = *obuf;
            }
            *nbuf = NULL;
      }
      return str;
}

void stripext (char *in, char *out)
{
	while(*in && *in != '.')
		*out++ = *in++;
	*out=0;
}
