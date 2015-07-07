/*	dem2lf.c

	(C)opyright 2000 PMC TFW
	All rights reserved.
	 			
	Notes:

	* DEM altitude below 0 gets truncated to 0. Sealevel is at 0 by default.
	* ground altitude larger than 32766 gets truncated to 32767.
	
	Worklist:
	
	* settable sealevel - you can have less and more water - could be fun
	* add more options, for resolution and all
	* dynamic memory allocation for structures
	* clean out code to make it less "perlish"
	* add curve correction routines
	* little endian/big endian support (byteswap)
	* check .fmt file for correct format
	* check header grid_size, elev_m_unit and pals for correct values
*/

#include <math.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <time.h>
#include <ctype.h>

#include "md5.h"

// L0,L1,L2,L3,L4,L5
unsigned int tileres[6][8] = {
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0, 48, 160, 68, 661, 661, 694, 694},
{9, 215, 134, 274, 4467, 4467, 5866, 5866},
{25849, 46435, 40965, 26038, 32347, 32347, 39034, 39034},
{25849, 29353, 40965, 26038, 29739, 29739, 29572, 29572}};

// altitude data for whole region

#define MAX_ALTDATA 256

int altdata[MAX_ALTDATA];
int altcount;

// file handles

FILE *demfile;
FILE *lffile;
FILE *offile;

//
int argc;
char **argv;

// Ton's of global variables - thanks to Perl :-)

unsigned long offset;
unsigned int *tiles;
int ofile;
int nregions;
int ndups;

int resolution;
int nrows;
int ncols;

int bpl;
float bytesperssqx;
float bytesperssqy;
float bpsectx;
float bpsecty;

// stupid statistics
time_t starttime;
//int highest;
//int lowest;

#define MAX_HASHES 65535

struct dighash {
	md5_byte_t md5[16];
	unsigned long offs;
} digests[MAX_HASHES];

unsigned long digicount;

/* Let's get this thing rolling ... */

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
		} else if(strncmp("r",&argv[i][2],1)==0) {
			i++;	// bypass -r XX
		}
	}
	return NULL;	
}

void fileerror(char *msg) 
{
	printf("\n");
	perror(msg);
	if (demfile) fclose(demfile);
	if (lffile) fclose(lffile);
	if (offile) fclose(offile);
	exit(1);
}

void error(char *msg) 
{
	// fixme: add args

	printf("\n");
	printf("%s",msg);
	if (demfile) fclose(demfile);
	if (lffile) fclose(lffile);
	if (offile) fclose(offile);
	exit(2);
}

void stripext (char *in, char *out)
{
	while(*in && *in != '.')
		*out++ = *in++;
	*out=0;
}

unsigned int swapbytes(unsigned int swap) 
{
	unsigned char b1,b2;
	b1 = swap & 255;
	b2 = (swap>>8) & 255;
	return (b1<<8) + b2;
}

float getaltitude(unsigned char *buffer) 
{
	short alt;
	float alti;

// Cut out the crap and just put it in!
	memcpy(&alt,&buffer[0],2);

//	alt=swapbytes(alt);
// Talk about weirdness in C :-)

	alti=alt;

//	printf("%.2f\n",alti);
// We want to be more accurate - so let's float!

	alti*=3.280840;
	
	return(alti);
}

int compare_digest(md5_byte_t *digi1, md5_byte_t *digi2)
{
//	int i;

	if (memcmp(digi1,digi2,16)==0) return 1;
	return 0;

/*	
	for (i=0;i<16;i++)
		if(digi1[i]!=digi2[i]) return 0;

	return 1;
*/

}

void add_digest_value(md5_byte_t *digest,unsigned long off) 
{
//	int i;

	if (digicount>=MAX_HASHES) {
		printf("\nMax hash count exceeded (%ld)\n",digicount);
		printf("You may force processing this file with some option\n");
		printf("but duplicate regions will be present in resulting offile\n");
		error("MAX_HASHES exceeded\n");
	}

	memcpy(&digests[digicount].md5[0],&digest[0],16);
	
/*	for (i=0;i<16;i++)
		 digests[digicount].md5[i]=digest[i];
*/
	digests[digicount].offs=off;
	digicount++;
	return;
}

int check_digest(md5_byte_t *digi)
{
	int i;
	for (i=0;i<digicount;i++) 
		if (compare_digest(digi,digests[i].md5)) return i+1;
	return 0;
}

void writeregion(void) 
{
	const unsigned char fillup[3]={254,63,63};
	
	int result,i;
	unsigned int tile;
	int alt;
	
	md5_state_t state;
	md5_byte_t digest[16];
	
	unsigned long off;
	long written;

	nregions++;

// MD5 hash is used to check for duplicate sectors
	
	md5_init(&state);
	md5_append(&state,(md5_byte_t *)&altdata[0],altcount*sizeof(int));
	md5_finish(&state,digest);

	fflush(stdout);
	
	result=check_digest(digest);
	if (result) {	// Yoohoo, we already have exactly same kind of sector
		off=digests[result-1].offs;
		written=fwrite(&off,4,1,offile);	// write index
		if (written!=1) fileerror("offile write failed\n");
//		printf("X");
		ndups++;
	} else {	// no dupe sector found, let's do new
		add_digest_value(digest,offset);

		written=fwrite(&offset,4,1,offile);	// write index
		if (written!=1) fileerror("offile write failed\n");
		
		for (i=0;i<altcount;i++) {
			alt=altdata[i];
			if (alt<=0) tile=tiles[0];
			else
			if (alt<160) tile=tiles[1];
			else
			if (alt<660) tile=tiles[2];
			else
			if (alt<1640) tile=tiles[3];
			else
			if (alt<3280) tile=tiles[4];
			else
			if (alt<6560) tile=tiles[5];
			else
			if (alt<11480) tile=tiles[6];
			else tile=tiles[7];
			
			written=fwrite(&tile,2,1,lffile);
			if (written!=1) fileerror("lffile write error\n");
			
			written=fwrite(&alt,2,1,lffile);
			if (written!=1) fileerror("lffile write error\n");
						
			written=fwrite(&fillup,3,1,lffile);
			if (written!=1) fileerror("lffile write error\n");
			offset+=7;
		}
//		printf(".");
	}
	
	altcount=0;
}

void getregion(int Y, int X) 
{
	int x,y;
	float res;

	float poff,xoff,yoff;

	unsigned char buffer[2];
	unsigned long luettu;
	
//	printf("Region %.2f is X:%d Y:%d %.2f %.2f %.2f %.2f\n",(float)X*resolution+Y,X,Y,bpsectx,bytesperssqx,bpsecty,bytesperssqy);

	for (y=15;y>=0; y--) {
		for (x=0;x<16;x++) {
			xoff =(int) (X * bpsectx + x * bytesperssqx);
			yoff =(int) (Y * bpsecty + y * bytesperssqy);

			poff = (int)(yoff * bpl + xoff);
			
//			printf(" %d %d at %.2f %.2f %.2f\n",X,Y,xoff,yoff,poff);

			if (fmod(poff,2)==1) {
				if (X == 0) poff++;
				else poff--;
			}

			fseek(demfile,poff,0);
			luettu=fread(buffer,2,1,demfile);
			if (luettu!=1) fileerror("Error reading .DEM file\n");
			
			res=getaltitude(buffer);
//			printf("Find Region %d %d at %.2f %.2f %.2f %.2f\n",X,Y,xoff,yoff,poff,res);
			if (res<0) res=0; 	// we've nothing under sealevel!
			if (res>32767) res=32767; // truncate too high stuffs
			
			if (altcount>=MAX_ALTDATA) error("Region size exceeds reserved space MAX_ALTCOUNT\n");
			
			altdata[altcount]=res;
			++altcount;
		}
	}
	return;
}

void region(void) 
{
	int x,y;

	printf("\n");
	for (x=resolution-1; x>=0;x--) {
		printf(".");
//		printf("R%d: ",x);
		for (y=0;y<resolution; y++) {
			getregion(x,y);
			writeregion();
		}
//		printf("\n");
	}
	printf("\n");
	return;
}

int onepass(void) 
{
	unsigned char buffer[2];
	unsigned long off;
	int maxx=80,maxy=32;
	int y,x;
	float res;
	unsigned long luettu;

//	printf("XStep %f YStep %f\n",(float)nrows/maxx,(float)nrows/maxy);
	for (y=0;y<maxy;y++) {
		for (x=0;x<maxx;x++) {
			off=ncols / maxx * x * 2 + nrows/maxy * y * bpl;
//			printf("X: %d Y:%d OFF:%ld\n",x,y,off);
			fseek(demfile,off,SEEK_SET);
		
			luettu=fread(buffer,2,1,demfile);
			if (luettu!=1) error("Error reading .DEM file\n");
	
			res = getaltitude(buffer);
			if (res<0) res=0;
			if (res>0) printf("@");
			else printf(" ");
			
		}
		printf("\n");
	}
	return 0;
}

int readheader(char *barefname)
{
// fixme: do this right

	char tmpfname[255];
	FILE *tmpfile;
	char line[255];
	char *tok;
	char hopt[255];
	char hargs[255];
	
	sprintf(tmpfname,"%s.hdr",barefname);
	tmpfile=fopen(tmpfname,"rt");
	if(!tmpfile) {
		printf("DEM header not found, using defaults.\n");
		return 1;
	}
	printf("Reading init values from %s\n",tmpfname);
	while (!feof(tmpfile)) {
		fgets(line,255,tmpfile);

		// "Do not never ever use strtok" !-)
		// fixme: use other method (lineparse)
		
		if(strlen(line)<1) break;
		rmallws(line);
		tok=strtok(line,"=");
		if(!tok) break;
		strncpy(hopt,tok,254);
//		printf("Header: %s",hopt);
		tok=strtok(NULL,"=");
		if (!tok) break;
		strncpy(hargs,tok,254);
//		printf("Argument: %s\n",hargs);

// remove all whitespaces!
// rmallws(hargs);
// rmallws(hopt);		
		if (strncmp(hopt,"number_of_columns",19)==0) ncols=atoi(hargs);
		if (strncmp(hopt,"number_of_rows",14)==0) nrows=atoi(hargs);	
		if (strncmp(hopt,"elev_m_unit",11)==0)
			if(strncmp(hargs,"meters",6)) printf("WARNING! DEM file altitude data is not meters!\n");		
		if (strncmp(hopt,"grid_size",9)==0)
			if (atof(hargs)!=0.00833333333) printf("WARNING! Invalid grid size in DEM\n");	
	}
	
	fclose(tmpfile);
	return 0;	
}

void showhelp(void)
{
		printf("Usage: dem2lf [options] filename [outputfile]\n");
		printf("\nOptions:\n");
		printf("\t-256\t256*256\n");
		printf("\t-128\t128*128\n");
		printf("\t-64\t64*64\n");
		printf("\t-32\t32*32\n");
		printf("\t-16\t16*16\n");
		printf("\t-8\t8*8\n");	
//		printf("\t-r x\tset resolution to x\n");
//		printf("\t-s x\tset sealevel to x\n");
//		printf("\t-f\t force processing even with some errors\n");
		printf("\t-skipheader\tdo not read header file\n");
//		printf("\t-showpass\tshow onepass map\n");
//		printf("\t-q\tDon't show extra crap\n");
		printf("\nExample: dem2lf -64 vietnam4.bin theater\n");
		printf("Will create theater.o2 and theater.l2 files with 64x64 res from vietnam4.bin\n\n");
}


int main(int eargc, char **eargv) 
{
	int i;
	char fname[255];
	char barefname[255];
	char tmpfname[255];
	char ofname[255];
	
	char *test;

	printf("DEM to Falcon4 LF converter v0.121\n");
	printf("(C)opyright 2000 PMC TFW <tactical@nekromantix.com>\n");
	printf("http://tactical.nekromantix.com\n");
	printf("--------------------------------------------------\n");

// Lets do ARGS
	argc=eargc;
	argv=eargv;
	
	if (argc<2 || Check_Param("-h") || Check_Param("-?")) {
		showhelp();
		exit(0);	
	}
	ofile=2; // value given with - parameter

/*	
	if (strncmp(argv[1],"-",1)==0) {
		if (strlen(argv[1])>1 && isdigit(argv[1][1]))
			ofile=atoi(&argv[1][1]);
	}
*/


// fixme: clean up this ofile mess someday
// well, we need these to be Perl version compatible
	if (Check_Param("-0")) ofile=0;
	if (Check_Param("-1")) ofile=1;
	if (Check_Param("-2")) ofile=2; 
	if (Check_Param("-3")) ofile=3;
	if (Check_Param("-4")) ofile=4;
	if (Check_Param("-5")) ofile=5;

// and these just for kicks, could put resolution directly ... who gives..
	if (Check_Param("-256")) ofile=0;
	if (Check_Param("-128")) ofile=1;
	if (Check_Param("-64")) ofile=2; 
	if (Check_Param("-32")) ofile=3;
	if (Check_Param("-16")) ofile=4;
	if (Check_Param("-8")) ofile=5;
	
// ... but we can improve, of course :)	
	
	if ((i=Check_Param("-r"))) {
		printf("Setting resolution to: XX x XX\n");

		if (argv[1] && isdigit(argv[i+i][1])) {
			resolution=atoi(argv[i+1]);
//			ofile=pow(2,resolution)/256;
//			ofile=pow(2,sqrt(256/resolution));
			// solve this damn riddle	
		}
	}

//	if (Check_Param("-q")) nodisplay=1;

//	printf("ofile: %d\n",ofile);

	if(ofile<0 || ofile>5) exit(1);

	test=Get_Next_Name();
	strcpy(ofname,"");
	
	if (test) strncpy(fname,test,254);
	else exit(1);
	
	while(test) {
//		printf("File: %s\n",test);
		test=Get_Next_Name();
		if (test) strncpy(ofname,test,254);
	}

// Initialize L0 & L1

	for(i=0;i<8;i++) {
		tileres[0][i]= tileres[2][i]+8192;
		tileres[1][i]=tileres[2][i]+4096;
//		printf("%d\n%d\n",tileres[0][i],tileres[1][i]);
	}

/*
//	Well, if I ever want to start going Dynamic .. 
	int *altdata;
	altdata=malloc(MAX_ALTDATA*sizeof(int));
	dighash *digests;
	digests=malloc(MAX_HASHES*sizeof(dighash));
	*/

// Init generic values

	offset=0;
	nregions=0;
	ndups=0;
	altcount=0;
	digicount=0;

// byteswap

// Init map specific variables
	tiles=tileres[ofile];
	resolution = 256 / (pow(2,ofile));
	nrows = 1200;
	ncols = 1200;

// Progress header file	
	
	stripext(fname,barefname);

	if(!Check_Param("-skipheader")) readheader(barefname);

// Calculate map structural values

	bpl = ncols * 2;
	bytesperssqx = (float)ncols / (16*resolution) * 2;
	bytesperssqy =(float) nrows / (16*resolution);
	bpsectx = (float)ncols / resolution * 2;
	bpsecty = (float)nrows / resolution;

//	printf("Size is %d x %d\n",nrows,ncols);
//	printf("Resolution is %d x %d\n\n",resolution,resolution);

	printf("Rows: %d Cols: %d\n",nrows,ncols);
	printf("%d * %d resolution\n",resolution,resolution);
// start file processing

	printf("Reading %s dem data\n",fname);

	demfile=fopen(fname,"rb");
	if(!demfile) fileerror("Could not open DEM file for reading ");

	// fixme: change sprintf -> snprintf
	if (strlen(ofname)>0) sprintf(tmpfname,"%s.o2",ofname);
//	else sprintf(tmpfname,"%s.o%d",barefname,ofile);
	else sprintf(tmpfname,"%s.o2",barefname);

	printf("Writing index to %s\n",tmpfname);

	offile=fopen(tmpfname,"wb");
	if(!offile) fileerror("Could not open offile for writing ");

	if (strlen(ofname)>0) sprintf(tmpfname,"%s.l2",ofname);
//	else sprintf(tmpfname,"%s.l%d",barefname,ofile);
	else sprintf(tmpfname,"%s.l2",barefname);

	printf("Writing lfdata to %s\n",tmpfname);
	
	lffile=fopen(tmpfname,"wb");
	if(!lffile) fileerror("Could not open lffile for writing ");

// Ready to rock'n'roll

	starttime=time(NULL);	
//	if (Check_Param("-showpass")) onepass();
	region();

	fclose(offile);
	fclose(lffile);
	fclose(demfile);
	printf("\n%ld hashes %d duplicates\n",digicount,ndups);
//	printf("%d Regions produced (%d duplicates)\n",nregions,ndups);	
//	printf("Highest alt: %d feet lowest: %d feet\n",highest,lowest);
	printf("Processing completed in %ld seconds\n",time(NULL)-starttime);
	return 0;
}
	