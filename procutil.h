#define DELIMITER 1
#define STRING 2
#define NUMBER 3

extern int Check_Param(char *parm);
extern void *Get_Next_Name(void);
extern void *Get_Next_Option(void);
extern void get_token(void);
extern char *rmallws(char *str);
extern void stripext(char *in, char *out);

extern unsigned char *p;
extern char token[80];
extern char tok_type;