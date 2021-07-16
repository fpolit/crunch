#include <assert.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>

/* largest output string */
#define MAXSTRING 128
/* longest character set */
#define MAXCSET 256

/* invalid index for size_t's */
#define NPOS ((size_t)-1)

static const wchar_t def_low_charset[] = L"abcdefghijklmnopqrstuvwxyz";
static const wchar_t def_upp_charset[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const wchar_t def_num_charset[] = L"0123456789";
static const wchar_t def_sym_charset[] = L"!@#$%^&*()-_+=~`[]{}|\\:;\"'<>,.?/ ";
static const char version[] = "3.6";

static size_t inc[128];
static size_t numofelements = 0;
static size_t inverted = 0;  /* 0 for normal output 1 for aaa,baa,caa,etc */

static unsigned long long linecount = 0; /* user specified break output into count lines */

static unsigned long long bytecount = 0 ;  /* user specified break output into size */

static volatile sig_atomic_t ctrlbreak = 0; /* 0 user did NOT press Ctrl-C 1 they did */

static FILE *fptr;        /* file pointer */

static int output_unicode = 0; /* bool. If nonzero, all output will be unicode. Can be set even if non-unicode input*/

/* When unicode input is given, the code cannot [currently] always calculate
  the exact final file size.  In those cases this var simply suppresses
  the output.  For what it's worth, the calculated size is always the minimum size. */
static int suppress_finalsize = 0; /* bool */

/*
Need a largish global buffer for converting wide chars to multibyte
to avoid doing an alloc on every single output line
leading to heap frag.  Only alloc'd once at startup.
Size is MAXSTRING*MB_CUR_MAX+1
*/
static char* gconvbuffer = NULL;
static size_t gconvlen = 0;

struct thread_data{
  unsigned long long finalfilesize; /* total size of output */
  unsigned long long bytetotal;  /* total number of bytes so far */
  unsigned long long bytecounter; /* count number of bytes in output resets to 0 */
  unsigned long long finallinecount; /* total size of output */
  unsigned long long linetotal; /* total number of lines so far */
  unsigned long long linecounter; /* counts number of lines in output resets to 0 */
};

/* pattern info */
struct pinfo {
  wchar_t *cset; /* character set pattern[i] is member of */
  size_t clen;
  int is_fixed; /* whether pattern[i] is a fixed value */
  size_t start_index, end_index; /* index into cset for the start and end strings */
  size_t duplicates;
};

/* program options */
struct opts_struct {
  wchar_t *low_charset;
  wchar_t *upp_charset;
  wchar_t *num_charset;
  wchar_t *sym_charset;
  size_t clen, ulen, nlen, slen;
  wchar_t *pattern;
  size_t plen;
  wchar_t *literalstring;
  wchar_t *startstring;
  wchar_t *endstring;
  size_t duplicates[4]; /* allowed number of duplicates for each charset */

  size_t min, max;

  wchar_t *last_min;  /* last string of length min */
  wchar_t *first_max; /* first string of length max */
  wchar_t *min_string;
  wchar_t *max_string; /* either startstring/endstring or calculated using the pattern */

  struct pinfo *pattern_info; /* information generated from pattern */
};
typedef struct opts_struct options_type;

static struct thread_data my_thread;


static int wcstring_cmp(const void *a, const void *b);
static void ex_program();
static size_t force_wide_string(wchar_t *wout, const char *s, size_t n);
static size_t make_wide_string(wchar_t *wout, const char *s, size_t n, int* r_is_unicode);
static wchar_t *alloc_wide_string(const char *s, int* r_is_unicode);
static size_t force_narrow_string(char *out, const wchar_t* src, size_t n);
static size_t make_narrow_string(char *out, const wchar_t* src, size_t n);
static int getmblen(wchar_t wc);
static wchar_t *dupwcs(const wchar_t *s);
static int check_member(const wchar_t *string1, const options_type* options);
static size_t find_index(const wchar_t *cset, size_t clen, wchar_t tofind);
static void fill_minmax_strings(options_type *options);
static void fill_pattern_info(options_type *options);
static unsigned long long fill_next_count(size_t si, size_t ei, int repeats, unsigned long long sum, /*@null@*/ unsigned long long *current_count, /*@out@*/ unsigned long long *next_count, size_t len);
static unsigned long long calculate_with_dupes(int start_point, int end_point, size_t first, size_t last, size_t pattern_index, int repeats, unsigned long long current_sum, /*@null@*/ unsigned long long *current_count, const options_type options, size_t plen);
static unsigned long long calculate_simple(const wchar_t *startstring, const wchar_t *endstring, const wchar_t *cset, size_t clen);
static void count_strings(unsigned long long *lines, unsigned long long *bytes, const options_type options);
static int finished(const wchar_t *block, const options_type options);
static int too_many_duplicates(const wchar_t *block, const options_type options);
static void increment(wchar_t *block, const options_type options);
static void *PrintPercentage(void *threadarg);
static void renamefile(const size_t end, const char *fpath, const char *outputfilename, const char *compressalgo);
static void printpermutepattern(const wchar_t *block2, const wchar_t *pattern, const wchar_t *literalstring, wchar_t **wordarray);
static void Permute(const char *fpath, const char *outputfilename, const char *compressalgo, wchar_t **wordarray, const options_type options, const size_t sizePerm, size_t unchanged);
static void Permutefilesize(wchar_t **wordarray, const size_t sizePerm, const size_t length, size_t unchanged);
static void loadstring(wchar_t *block2, const size_t j, const wchar_t *startblock, const options_type options);
static void chunk(const size_t start, const size_t end, const wchar_t *startblock, const options_type options, const char *fpath, const char *outputfilename, const char *compressalgo);
static void usage();
static wchar_t *resumesession(const char *fpath, const wchar_t *charset);
static wchar_t *readcharsetfile(const char *charfilename, const char *charsetname, int* r_is_unicode);
static wchar_t **readpermute(const char *charfilename, int* r_is_unicode);
static void copy_without_dupes(wchar_t* dest, wchar_t* src);
