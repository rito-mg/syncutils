/* getopt.h
 * 
 */

#ifndef kGetoptBadChar

#define kGetoptBadChar   ((int) '?')
#define kGetoptErrMsg    ""

#ifndef getopt_c
extern int gOptErr;					/* if error message should be printed */
extern int gOptInd;					/* index into parent argv vector */
extern int gOptOpt;					/* character checked for validity */
extern const char *gOptArg;			/* argument associated with option */
#endif

#ifdef __cplusplus
extern "C" {
#endif

void GetoptReset(void);
int Getopt(int nargc, const char **const nargv, const char *const ostr);

#ifdef __cplusplus
}
#endif

#endif

