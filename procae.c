/*	procppp.c

	(C)opyright 2000 PMC TFW
	All rights reserved.
	
	Notes:
	not much, quickly made and ugly as hell :-)

	Use -un parameter to also include "nameless" features, like
	some airfields which don't have names in .e00 file - I don't know
	why, perhaps they're military airfields and don't have real name :)

*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <math.h>

#include <string.h>

#include "procutil.h"

// Ton's of global vars again - damn perl!-)

int phase=0;

char type[255];
int range;
double longbase;
double latbase;

int fileoutput=0;
int lopeta;

unsigned long totalitems;
unsigned long totalcoords;
unsigned long skippeditems;

int argc;
char **argv;

// Hmm, I wonder how many features can one map have...

#define MAX_COORDS 8192

struct {
	unsigned char type;
	double x;
	double y;
	char name[40];
	int id;
} coords[MAX_COORDS];

int coordcount;

void fileerror(char *msg) 
{
	printf("\n");
	perror(msg);
	exit(1);
}

int addcoord(int id,double x,double y)
{

	x-=longbase;
	y-=latbase;

	if(x<0 || x>range || y<0 || y>range) return 1;

// Do we need more accurate covnersion?
	x=(int)(x/range*4096);
	y=(int)(y/range*4096);

	if (coordcount>=MAX_COORDS) {
		printf("Too many features..\n");
		exit(1);
	}
	coords[coordcount].x=x;
	coords[coordcount].y=y;
	coords[coordcount].id=id;
	strcpy(coords[coordcount].name,"");
	++coordcount;
	
//	printf("Success Add: %d %d,%d\n",id,(int)x,(int)y);
	return 0;
}

void parse_expo(void) 
{
// fixme: quick and dirty...
	char name[40];
	char temp[40];
	int idcode,i;
	unsigned char type;

	if (phase<2) return;	// skip dupe coords

	strcpy(name,"");
	strcpy(temp,"");

// skip exp zeroes
	get_token();	
	if(!*token) return;

// this one should be id number
	get_token();
	if(!*token) return;

	if (tok_type==NUMBER) {
		idcode=atoi(token);
	} else return;

// this is coverage id, not used in falcon	
	get_token();	
	if(!*token) return;	

// next should be name...	
	get_token();	// this should be name
	if(!*token) return;

	type=*token;
	
	strncpy(temp,&token[1],39);

// fixme: strcat -> strncat
	strcat(name,temp);	
	while(*token) {
		if (tok_type==STRING ) {
			sprintf(temp," %s",token);
			strcat(name,temp);
		}
		get_token();		
	}
	
	for (i=0;i<coordcount;i++) {
		if (coords[i].id==idcode) {
			strncpy(coords[i].name,name,39);
			coords[i].type=type;
			return;
		}
	}
// not found
	return;
};

int howmany=0;

void parse_datahdr(void) 
{
// fixme: quick and dirty...
	int id;
	double X,Y;

	if (phase==0) {		// phase=0, we're at the first LAB section
		if(!*token) return;
		if(tok_type!=NUMBER) return;

		id=atoi(token);

		if (id==-1) {
			phase=1;
			return;
		}

// Let's get ID... 
		get_token();
		if(!*token) return;
		if(tok_type!=NUMBER) return;
		id=atoi(token);
// and X coord...		
		get_token();
		if(!*token) return;
		if(tok_type!=NUMBER) return;
		X=atof(token);
// and Y coord...		
		get_token();
		if(!*token) return;
		if(tok_type!=NUMBER) return;
		Y=atof(token);
// then add them to list...		
		addcoord(id,X,Y);						
	}
	return;
}

void parse_strings(void)
{
// fixme: quick and dirty... phase=2 when we're near those coord/name pairs
	if(strncmp(token,"EOP",3)==0) phase=2;
	if(strncmp(token,"Paramete",8)==0) phase=2;	
	return;
}

int readstuffs(char *fname) {
	FILE *tmpfile;
	char line[255];
	
	unsigned long lines=0;
	phase=0;
	coordcount=0;
	
	lopeta=0;
	tmpfile=fopen(fname,"rt");
	if (!tmpfile) fileerror("Error opening source file...");
	while(!feof(tmpfile) && !lopeta) {
		fgets(line,255,tmpfile);
		if(strlen(line)<1) break;
		++lines;
		p=line;
		get_token();
		if (*token) {
			if (tok_type==STRING) parse_strings();
			else
			if (tok_type==NUMBER && strchr(token,'+')) parse_expo();
			else if(tok_type==NUMBER) parse_datahdr();
		// skip everything else
		}
	}
	fclose(tmpfile);
	return 0;
}

void showhelp(void)
{
		printf("Usage: procppp [options] type filename [outputfile]\n");
		printf("\nOptions:\n");
//		printf("\t-f\tOutput to file instead of doing screen output\n");
		printf("\t-un\nInclude unnamed items in output too\n");
		printf("\nExample: procae -f Airport airport\n");
}


int main(int eargc, char **eargv) {
	char fname[255];
	char *test;
	
	int unnamed=0,listunnamed=0;
	int i;
	
	char *tok;
	char hopt[255];
	char hargs[255];		

	longbase=102.0;
	latbase=12.0;
	range=40;

// Lets do ARGS

	argc=eargc;
	argv=eargv;
	totalitems=0;
	totalcoords=0;
	skippeditems=0;

	if (argc<2 || Check_Param("-h") || Check_Param("-?")) {
		showhelp();
		exit(0);	
	}

	if (Check_Param("-f")) fileoutput=1;		
	if (Check_Param("-un")) listunnamed=1;
	if (fileoutput) {
		printf("Point E00 to Falcon4 features converter v0.1\n");
		printf("(C)opyright 2000 PMC TFW <tactical@nekromantix.com>\n");
		printf("http://tactical.nekromantix.com\n");
		printf("--------------------------------------------------\n");
	}	
	
	test=Get_Next_Option();
	while (test) {
		if (strchr(test,'=')) {
		// fixme: "Do not never ever use strtok" !-)
			rmallws(test);
			tok=strtok(test,"=");
			if(!tok) continue;
			strncpy(hopt,tok,254);
			tok=strtok(NULL,"=");
			if (!tok) continue;
			strncpy(hargs,tok,254);
			if (strncmp(hopt,"-range",6)==0) range=atoi(hargs);
			else if (strncmp(test,"-long",5)==0) longbase=atol(hargs);
			else if (strncmp(test,"-lat",4)==0) latbase=atol(hargs);
		}
		test=Get_Next_Option();
	}	

	test=NULL;

	test=Get_Next_Name();
	if(!test) exit(1);
	strncpy(type,test,254);

	test=Get_Next_Name();	
	if(!test) exit(1);
	strncpy(fname,test,254);

	if (fileoutput) printf("Reading E00 point data from: %s\n",fname);

	readstuffs(fname);

	unnamed=0;
	for (i=0;i<coordcount;i++) {
		if(strlen(coords[i].name)>0)
			printf("%s 1 %d,%d %c %s\n",type,(int)coords[i].x,(int)coords[i].y,coords[i].type,coords[i].name);
		else {
			unnamed++;
			if (listunnamed) printf("%s 1 %d,%d %c %s #%d\n",type,(int)coords[i].x,(int)coords[i].y,coords[i].type,type,i+1);
		}
	}

	if (fileoutput) {
		printf("\nTotal %d items (%d unnamed, skipped)\n",coordcount,unnamed);
		printf("Processing completed\n\n");
	}
	return 0;
}

