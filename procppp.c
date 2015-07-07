/*	procppp.c

	(C)opyright 2000 PMC TFW
	All rights reserved.

	Notes:
	
	Quick and ugly - that's what this one is...
		
	There is one totally weird thing with some .e00 files. Sometimes extra 
	coordinates are present in the file... even "number of coordinates" 
	in header says there are more/less coordinates. Current action is 
	to do like Perl version does, truncate FIRST coordinate pair... 
	which is bit stupid if you ask me...
	
	If we don't truncate, maps get messed up...
		
	Also, this version may produce little more roads/rivers than perl
	version, probably due accuracy differences...
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <math.h>

#include <string.h>

#include "procutil.h"

// Ton's of global vars again - damn perl!-)
FILE *tdffile;

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

#define MAX_COORDS 2048

struct {
	double x;
	double y;
} coords[MAX_COORDS];

int coordcount;

void fileerror(char *msg) 
{
	printf("\n");
	perror(msg);
	if (tdffile) fclose(tdffile);
	exit(1);
}

int addcoord(double x,double y)
{

	x-=longbase;
	y-=latbase;

// If it's not insize this zone, we don't want it
	if(x<0 || x>range || y<0 || y>range) return 1;

// Do we need more accurate conversion?

	x=(int)(x/range*4096);
	y=(int)(y/range*4096);
	if (coordcount>=MAX_COORDS) {
		printf("Too many features..\n");
		exit(1);
	}
	coords[coordcount].x=x;
	coords[coordcount].y=y;
	++coordcount;
	
//	printf("Coord: %d %d\n",(int)x,(int)y);
	return 0;
}

void parse_coordinates(void) 
{
	double X=0,Y=0,test=0;
	int getxgety=0;

	while(*token) {
		if(tok_type==NUMBER) {
			if(strchr(token,'+')) {
				test=atof(token);
				++totalcoords;
				// fixme: do this right
				if (getxgety==0) {
					X=test;
					getxgety=1;
				} else {
					Y=test;
					addcoord(X,Y);						
					getxgety=0;
				}	
			} 
			//else printf("+ not found\n");		
		} 
		get_token();	
	}

//	if(getxgety==1) exit(1); // only x coord!
	return;
};

int howmany=0;

void parse_roadhdr(void) 
{
/* E00 ARC part format is as follows:
	coverage #
	coverage-id
	from node
	to node
	left polygon
	right polygon
	number of coords
*/
	int i,start=0;
	int number=0;

	if (coordcount>1 && howmany) {
		if(coordcount<howmany) howmany=coordcount;
	
		if (fileoutput) fprintf(tdffile,"%s %d",type,coordcount);
//		else printf("%s %d",type,coordcount);
		else printf("%s %d",type,howmany);

//		this is what I call weird... sometimes there are EXTRA coordinates present
//		more than amount said to be... current action is same as in perl, truncate first pair
//
		if (howmany!=coordcount) start=1;
//		for (i=0;i<coordcount;i++)
		for (i=start;i<coordcount;i++)
			if (fileoutput) fprintf(tdffile," %d,%d",(int)coords[i].x,(int)coords[i].y);
			else printf(" %d,%d",(int)coords[i].x,(int)coords[i].y);
		
		if (fileoutput) fprintf(tdffile,"\n");
		else printf("\n");
		
//		if (howmany!=coordcount) printf("Different\n");

		coordcount=0;
		howmany=0;	
		++totalitems;
	}

// We go thru remaining stuffs in line and what we want is value
// from last of the items...
// fixme: do this right with some checking	
	while(*token) {
		if(tok_type==NUMBER) {
			number=atoi(token);
			if (number==-1) {
				lopeta=1;
				break;
			}
		} 
		get_token();	
	}
	howmany=number;
		
	return;
}

int readstuffs(char *fname) {
	FILE *tmpfile;
	char line[255];

	unsigned long lines=0;
	
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
			if (tok_type==NUMBER && strchr(token,'+')) parse_coordinates();
			else if(tok_type==NUMBER) parse_roadhdr();
//			else printf("Other line\n");
		// skip everything else
		}
	}
	fclose(tmpfile);
	return 0;
}

void showhelp(void)
{
		printf("Polygon E00 to Falcon4 features converter v0.1\n");
		printf("(C)opyright 2000 PMC TFW <tactical@nekromantix.com>\n");
		printf("http://tactical.nekromantix.com\n");
		printf("--------------------------------------------------\n");
		printf("Usage: procppp [options] type filename [outputfile]\n");
		printf("\nOptions:\n");
//		printf("\t-f\tOutput to file instead of doing screen output\n");
		printf("\nExample: procppp -range10 -long=112.0 -lat=12 rdline.e00 >roads.tdf\n");
}

int main(int eargc, char **eargv) {
	char fname[255];
	char *test;
	char tdfname[255];
	
	char *tok;
	char hopt[255];
	char hargs[255];	


// stupid default values
	longbase=100.0;
	latbase=10.0;
	range=20;

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
	
	if (fileoutput) {
		printf("Polygon E00 to Falcon4 features converter v0.1\n");
		printf("(C)opyright 2000 PMC TFW <tactical@nekromantix.com>\n");
		printf("http://tactical.nekromantix.com\n");
		printf("--------------------------------------------------\n");
	}	
	test=Get_Next_Option();
	while (test) {
		if (strchr(test,'=')) {
		// fixme: "Do not never ever use strtok" :-)
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
// first we try to get TYPE	
	test=Get_Next_Name();
	if(!test) exit(1); // erm, not found
	strncpy(type,test,254);

// then we hope for source filename
	test=Get_Next_Name();	
	if(!test) exit(1); // not found..
	strncpy(fname,test,254);

// and from the remaing crap, if there is any, we create output filename
// (to be used, when fileoutput=1)
	test=Get_Next_Name();			
	while(test) {
		strncpy(tdfname,test,254);
		test=Get_Next_Name();		
	}

	if (!tdfname) {
		stripext(fname,test);
		sprintf(tdfname,"%s.tdf",test);
	}

	if (fileoutput) printf("Reading E00 data from: %s\n",fname);
	if(fileoutput) {	
		tdffile=fopen(tdfname,"wt");
		if(!tdffile) fileerror("Error creating tdf...");
		printf("Writing TDF data to: %s\n",tdfname);
		
	}
	
	readstuffs(fname);

	if (tdffile) fclose(tdffile);
	if (fileoutput) {
		printf("\nTotal %ld items %ld coordinates\n",totalitems,totalcoords);
		printf("Processing completed\n\n");
	}
	return 0;
}

