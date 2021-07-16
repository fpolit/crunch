#include "utils.h"

static int wcstring_cmp(const void *a, const void *b) {
  const wchar_t **ia = (const wchar_t **)a;
  const wchar_t **ib = (const wchar_t **)b;
  return wcscmp(*ia, *ib);
}

static void ex_program() {
  ctrlbreak = 1;
  (void) signal(SIGINT, SIG_DFL);
}

static size_t force_wide_string(wchar_t *wout, const char *s, size_t n) {
  size_t i;
  const unsigned char *ucp = (const unsigned char*)s;
  size_t slen = strlen(s);

  /*
  Blindly convert all characters to the numerically equivalent wchars.
  This is intended to be used after a call to mbstowcs fails.
  Like mbstowcs(), output may not be null terminated if returns n
  */

  for (i=0; i<n && i<slen; ++i) {
    wout[i] = (wchar_t)ucp[i];
  }

  if (i<n)
    wout[i] = 0;

  return i;
}

static size_t make_wide_string(wchar_t *wout, const char *s, size_t n, int* r_is_unicode) {
  size_t stres;
  const char* cp;
  int contains_upp128 = 0;

  /*
  If 's' contains a UTF-8 string which is not plain 7bit,
  is_unicode is set nonzero and wout contains the proper wide string.
  Otherwise the code points are assumed to be the exact values in s.
  Unlike mbstowcs, result is always null terminated as long as
  n is nonzero.
  Leave r_is_unicode undisturbed unless setting to nonzero!
    (must never be changed from 1 to 0 regardless of this call's data)
  */

  for (cp = s; *cp; ++cp) {
    if ((int)*cp < 0) {
      contains_upp128 = 1;
      break;
    }
  }

  stres = mbstowcs(wout,s,n);
  if (stres != NPOS) {
    if (contains_upp128 && r_is_unicode)
      *r_is_unicode = 1;
  }
  else
    stres = force_wide_string(wout,s,n);

  if (n != 0)
    wout[n-1]=0;

  return stres;
}

static wchar_t *alloc_wide_string(const char *s, int* r_is_unicode) {
  wchar_t* wstr = NULL;
  size_t len = s ? strlen(s)+1 : 1;

  wstr = (wchar_t*)malloc(len*sizeof(wchar_t));
  if (!wstr) {
    fprintf(stderr,"alloc_wide_string: Can't allocate mem!\n");
    exit(EXIT_FAILURE);
  }
  (void)make_wide_string(wstr,s?s:"",len,r_is_unicode);
  return wstr;
}

static size_t force_narrow_string(char *out, const wchar_t* src, size_t n) {
size_t i;
  for (i=0; i<n && src[i]!=L'\0'; ++i) {
    out[i] = (char)(src[i]&0xFF);
  }

  if (i<n)
    out[i] = '\0';

  return i;
}

static size_t make_narrow_string(char *out, const wchar_t* src, size_t n) {
size_t retval;

  /*
  If global output_unicode is true, src is converted to a UTF-8 string.
  If not, the low 8 bits are copied to the output string
  which is most definitely not what you want unless the input
  wasn't unicode in the first place.
  Unlike wcstomb(), output is always null terminated as long as n!=0
  */

  if (output_unicode != 0) {
    retval = wcstombs(out, src, n);
    if (retval == NPOS) {
      fprintf(stderr,"Error: wcstombs() failed.  This shouldn't have happened.\n");
      exit(EXIT_FAILURE);
    }
  }
  else
    retval = force_narrow_string(out, src, n);

  if (n!=0)
    out[n-1]='\0';

  return retval;
}

static int getmblen(wchar_t wc) {
  /* returns number of bytes required for wide char wc, taking into account current setting of global output_unicode */
  int mblen = 1;
  if (output_unicode) {
    char mb[MB_CUR_MAX+1];
    mblen = wctomb(mb,wc);
    if (mblen == -1) {
      fprintf(stderr,"Warning: wctomb failed for char U+%04lX\n",(unsigned long)wc);
      mblen = 1;
    }
  }
  return mblen;
}

static wchar_t *dupwcs(const wchar_t *s) {
  /* replacement for wcsdup, where not avail (it's POSIX) */
  size_t n;
  wchar_t *p = NULL;

  if (s != NULL)
  {
    n = (1 + wcslen(s)) * sizeof(wchar_t);
    if ((p = (wchar_t*)malloc(n))!=NULL)
      memcpy(p, s, n);
  }

  return p;
}

/* return 0 if string1 does not comply with options.pattern and options.literalstring */
static int check_member(const wchar_t *string1, const options_type* options) {
const wchar_t *cset;
size_t i;

  for (i = 0; i < wcslen(string1); i++) {
    cset = NULL;
    switch (options->pattern[i]) {
      case L'@':
        if (options->literalstring[i] != L'@')
          cset = options->low_charset;
        break;
      case L',':
        if (options->literalstring[i] != L',')
          cset = options->upp_charset;
        break;
      case L'%':
        if (options->literalstring[i] != L'%')
          cset = options->num_charset;
        break;
      case L'^':
        if (options->literalstring[i] != L'^')
          cset = options->sym_charset;
        break;
      default: /* constant part of pattern */
        break;
    }

    if (cset == NULL) {
      if (string1[i] != options->pattern[i])
        return 0;
      continue;
    }

    while (*cset)
      if (string1[i] == *cset)
        break;
      else
        cset++;
    if (*cset == L'\0')
      return 0;
  }
  return 1;
}

/* NOTE: similar to strpbrk but length limited and only searches for a single char */
static size_t find_index(const wchar_t *cset, size_t clen, wchar_t tofind) {
  size_t i;
  for (i = 0; i < clen; i++)
    if (cset[i] == tofind)
      return i;
  return NPOS;
}

static void fill_minmax_strings(options_type *options) {
  size_t i;
  wchar_t *last_min; /* last string of size min */
  wchar_t *first_max; /* first string of size max */
  wchar_t *min_string, *max_string; /* first string of size min, last string of size max */

  last_min = calloc(options->min + 1,sizeof(wchar_t));
  if (last_min == NULL) {
    fprintf(stderr,"fill_minmax_strings: can't allocate memory for last_min\n");
    exit(EXIT_FAILURE);
  }
  last_min[options->min] = L'\0';

  first_max = calloc(options->max + 1,sizeof(wchar_t));
  if (first_max == NULL) {
    fprintf(stderr,"fill_minmax_strings: can't allocate memory for first_max\n");
    exit(EXIT_FAILURE);
  }
  first_max[options->max] = L'\0';

  min_string = calloc(options->min + 1,sizeof(wchar_t));
  if (min_string == NULL) {
    fprintf(stderr,"fill_minmax_strings: can't allocate memory for min_string\n");
    exit(EXIT_FAILURE);
  }
  min_string[options->min] = L'\0';

  max_string = calloc(options->max + 1,sizeof(wchar_t));
  if (max_string == NULL) {
    fprintf(stderr,"fill_minmax_strings: can't allocate memory for max_string\n");
    exit(EXIT_FAILURE);
  }
  max_string[options->max] = L'\0';

  /* fill last_min and first_max */
  for (i = 0; i < options->max; i++) {
    if (options->pattern == NULL) {
      if (i < options->min) {
        last_min[i] = options->low_charset[options->clen-1];
        min_string[i] = options->low_charset[0];
      }
      first_max[i] = options->low_charset[0];
      max_string[i] = options->low_charset[options->clen-1];
    }
    else { /* min == max */
      min_string[i] = max_string[i] = last_min[i] = first_max[i] = options->pattern[i];
      switch (options->pattern[i]) {
      case L'@':
        if (options->literalstring[i] != L'@') {
          max_string[i] = last_min[i] = options->low_charset[options->clen - 1];
          min_string[i] = first_max[i] = options->low_charset[0];
        }
        break;
      case L',':
        if (options->literalstring[i] != L',') {
          max_string[i] = last_min[i] = options->upp_charset[options->ulen - 1];
          min_string[i] = first_max[i] = options->upp_charset[0];
        }
        break;
      case L'%':
        if (options->literalstring[i] != L'%') {
          max_string[i] = last_min[i] = options->num_charset[options->nlen - 1];
          min_string[i] = first_max[i] = options->num_charset[0];
        }
        break;
      case L'^':
        if (options->literalstring[i] != L'^') {
          max_string[i] = last_min[i] = options->sym_charset[options->slen - 1];
          min_string[i] = first_max[i] = options->sym_charset[0];
        }
        break;
      default:
        break;
      }
    }
  }

  options->last_min = last_min;
  options->first_max = first_max;

  if (options->startstring)
    for (i = 0; i < options->min; i++)
      min_string[i] = options->startstring[i];
  if (options->endstring)
    for (i = 0; i < options->max; i++)
      max_string[i] = options->endstring[i];

  options->min_string = min_string;
  options->max_string = max_string;
}

static void fill_pattern_info(options_type *options) {
  struct pinfo *p;
  wchar_t *cset;
  size_t i, clen, index, si, ei;
  int is_fixed;
  size_t dupes;

  options->pattern_info = calloc(options->max, sizeof(struct pinfo));
  if (options->pattern_info == NULL) {
    fprintf(stderr,"fill_pattern_info: can't allocate memory for pattern info\n");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < options->max; i++) {
    cset = NULL;
    clen = 0;
    index = 0;
    is_fixed = 1;
    dupes = options->duplicates[0];
    if (options->pattern == NULL) {
      cset = options->low_charset;
      clen = options->clen;
      is_fixed = 0;
    }
    else {
      cset = NULL;
      switch (options->pattern[i]) {
        case L'@':
          if (options->literalstring[i] != L'@') {
            cset = options->low_charset;
            clen = options->clen;
            is_fixed = 0;
            dupes = options->duplicates[0];
          }
          break;
        case L',':
          if (options->literalstring[i] != L',') {
            cset = options->upp_charset;
            clen = options->ulen;
            is_fixed = 0;
            dupes = options->duplicates[1];
          }
          break;
        case L'%':
          if (options->literalstring[i] != L'%') {
            cset = options->num_charset;
            clen = options->nlen;
            is_fixed = 0;
            dupes = options->duplicates[2];
          }
          break;
        case L'^':
          if (options->literalstring[i] != L'^') {
            cset = options->sym_charset;
            clen = options->slen;
            is_fixed = 0;
            dupes = options->duplicates[3];
          }
          break;
        default: /* constant part of pattern */
          break;
      }
    }

    if (cset == NULL) {
      /* fixed character.  find its charset and index within. */

      cset = options->low_charset;
      clen = options->clen;
      dupes = options->duplicates[0];

      if (options->pattern == NULL) {
        fprintf(stderr,"fill_pattern_info: options->pattern is NULL!\n");
        exit(EXIT_FAILURE);
      }

      if ((index = find_index(cset, clen, options->pattern[i])) == NPOS) {
        cset = options->upp_charset;
        clen = options->ulen;
        dupes = options->duplicates[1];
        if ((index = find_index(cset, clen, options->pattern[i])) == NPOS) {
          cset = options->num_charset;
          clen = options->nlen;
          dupes = options->duplicates[2];
          if ((index = find_index(cset, clen, options->pattern[i])) == NPOS) {
            cset = options->sym_charset;
            clen = options->slen;
            dupes = options->duplicates[3];
            if ((index = find_index(cset, clen, options->pattern[i])) == NPOS) {
              cset = NULL;
              clen = 0;
              dupes = (size_t)-1;
              index = 0;
            }
          }
        }
      }
      si = ei = index;
    }
    else {
      /*
      si = find_index(cset, clen, options->min_string[i]);
      */

      /* That can't be right.  Would only work when we have a pattern
          or min==max, neither is guaranteed here.  Or is it?  -jC */

      if (i < wcslen(options->min_string))
        si = find_index(cset, clen, options->min_string[i]);
      else
        si = 0;

      ei = find_index(cset, clen, options->max_string[i]);

      if (si == NPOS || ei == NPOS) {
        fprintf(stderr,"fill_pattern_info: Internal error: "\
          "Can't find char at pos #%lu in cset\n",
          (unsigned long)i+1);
        exit(EXIT_FAILURE);
      }
    }
    p = &(options->pattern_info[i]);
    p->cset = cset;
    p->clen = clen;
    p->is_fixed = is_fixed;
    p->start_index = si;
    p->end_index = ei;
    p->duplicates = dupes;
  }

#ifdef DEBUG
  printf("pattern_info duplicates: ");
  for (i = 0; i < options->max; i++) {
    p = &(options->pattern_info[i]);
    printf("(%d %d %d %d %d %d)", p->cset, p->clen, p->is_fixed, p->start_index, p->end_index, p->duplicates);
  }
  printf("\n");
#endif
}

static unsigned long long fill_next_count(size_t si, size_t ei, int repeats, unsigned long long sum, /*@null@*/ unsigned long long *current_count, /*@out@*/ unsigned long long *next_count, size_t len) {
size_t i;
unsigned long long nextsum = 0;

  for (i = 0; i < si; i++)
    next_count[i] = 0;

  if (repeats == 0 || current_count == NULL)
    nextsum = sum;
  else if (repeats == 1)
    for (; i <= ei; i++) {
      next_count[i] = sum - current_count[i];
      nextsum += next_count[i];
    }
  else /* repeats > 1 */
    for (; i <= ei; i++) {
      next_count[i] = current_count[i];
      nextsum += next_count[i];
    }

  for (; i < len; i++)
    next_count[i] = 0;

  return nextsum;
}

/* calculate the number of strings from start to end taking into account duplicate removal worst case: O(2^n), could be improved using memoize */
static unsigned long long calculate_with_dupes(int start_point, int end_point, size_t first, size_t last, size_t pattern_index, int repeats, unsigned long long current_sum, /*@null@*/ unsigned long long *current_count, const options_type options, size_t plen) {
/* start_point and end_point are bools */

  unsigned long long count[MAXCSET];
  unsigned long long *next_count;
  unsigned long long total = 0, sum = 0;
  size_t start_index, end_index;
  size_t i;

  if (first == NPOS || last == NPOS || first > last || pattern_index > plen) /* sanity check */
    return 0;

   /* check for too many duplicates */
  if (pattern_index > 0 && (size_t)repeats > options.pattern_info[pattern_index-1].duplicates) {
    return 0;
  }

  /* create the result count for pattern_index - 1 */
  if (pattern_index == 0) {
    sum = current_sum;
    next_count = NULL;
  }
  else if (pattern_index == 1 ||
      (options.pattern_info[pattern_index-1].cset != NULL &&
      options.pattern_info[pattern_index-2].cset != options.pattern_info[pattern_index-1].cset) ||
      (options.pattern_info[pattern_index-1].cset == NULL &&
      options.pattern[pattern_index-2] != options.pattern[pattern_index-1])) {
    /* beginning new cset */
    if (repeats > 1) {
      return 0;
    }
    if (options.pattern_info[pattern_index-1].cset != NULL) {
      for (i = 0; i < first; i++)
        count[i] = 0;
      for (; i <= last; i++)
        count[i] = current_sum;
      for (; i < options.pattern_info[pattern_index-1].clen; i++)
        count[i] = 0;
      next_count = count;
    } else
      next_count = NULL;
    sum = current_sum * (last - first + 1);
  }
  else if (options.pattern_info[pattern_index-1].cset == NULL &&
      options.pattern[pattern_index-2] == options.pattern[pattern_index-1]) {
    /* continuing unknown cset */
    sum = current_sum;
    next_count = NULL;
  }
  else {
    /* continuing previous cset */
    sum = fill_next_count(first, last, repeats, current_sum, current_count, count, options.pattern_info[pattern_index-1].clen);
    next_count = count;
  }

  if (sum == 0) {
    return 0;
  }

  if (pattern_index == plen) { /* check for the end of the pattern */
    return sum;
  }

  if (options.pattern_info[pattern_index].is_fixed) {
    start_index = end_index = options.pattern_info[pattern_index].start_index;
  }
  else {
    start_index = 0;
    end_index = options.pattern_info[pattern_index].clen - 1;
    if (start_point)
      start_index = options.pattern_info[pattern_index].start_index;
    if (end_point)
      end_index = options.pattern_info[pattern_index].end_index;
  }

  if (start_index == end_index) {
    /* [0..si..0](sp, ep) */
    if (repeats > 0)
      total += calculate_with_dupes(start_point, end_point, start_index, end_index, pattern_index + 1, repeats + 1, sum, next_count, options, plen);
    total += calculate_with_dupes(start_point, end_point, start_index, end_index, pattern_index + 1, 1, sum, next_count, options, plen);
  }
  else {
    if (start_point) {
      /* [0..si..0](sp, 0) */
      if (repeats > 0)
        total += calculate_with_dupes(start_point, 0, start_index, start_index, pattern_index + 1, repeats + 1, sum, next_count, options, plen);
      total += calculate_with_dupes(start_point, 0, start_index, start_index, pattern_index + 1, 1, sum, next_count, options, plen);
      start_index++;
    }

    if (end_point) {
      /* [0..ei..0](0, ep) */
      if (repeats > 0)
        total += calculate_with_dupes(0, end_point, end_index, end_index, pattern_index + 1, repeats + 1, sum, next_count, options, plen);
      total += calculate_with_dupes(0, end_point, end_index, end_index, pattern_index + 1, 1, sum, next_count, options, plen);
      end_index--;
    }

    if (start_index <= end_index) {  /* middle */
      /* [0..si..ei..0](0,0) */
      if (repeats > 0)
        total += calculate_with_dupes(0, 0, start_index, end_index, pattern_index + 1, repeats + 1, sum, next_count, options, plen);
      total += calculate_with_dupes(0, 0, start_index, end_index, pattern_index + 1, 1, sum, next_count, options, plen);
    }
  }

  return total;
}

/* calculate the number of strings from start to end O(n)  */
static unsigned long long calculate_simple(const wchar_t *startstring, const wchar_t *endstring, const wchar_t *cset, size_t clen) {
  unsigned long long total = 1;
  size_t index1, index2;

  for (; *endstring && *startstring; endstring++, startstring++) {
    for (index1 = 0; cset[index1] != L'\0' && cset[index1] != *endstring;)
      index1++;
    if (cset[index1] == L'\0')
      index1 -= 1;
    for (index2 = 0; cset[index2] != L'\0' && cset[index2] != *startstring;)
      index2++;
    if (cset[index2] == L'\0')
      index2 = 0;
    total = (total - 1) * (unsigned long long)clen + (unsigned long long)index1 - (unsigned long long)index2 + 1;
  }
  return total;
}

/* calculate the number of strings from start to end, inclusive, taking into account the pattern O(n) */
static unsigned long long calculate_with_pattern(const wchar_t *startstring, const wchar_t *endstring, const options_type options) {
  unsigned long long total_strings = 1;
  size_t temp, clen;
  const wchar_t *cset;
  size_t index1, index2;

  if (options.pattern == NULL) {
    return calculate_simple(startstring, endstring, options.low_charset, options.clen);
  }

  for (temp = 0; temp < options.plen; temp++) {
    cset = NULL;
    clen = 0;
    switch (options.pattern[temp]) {
      case L'@':
        if (options.literalstring[temp] != L'@') {
          clen = options.clen;
          cset = options.low_charset;
        }
        break;
      case L',':
        if (options.literalstring[temp] != L',') {
          clen = options.ulen;
          cset = options.upp_charset;
        }
        break;
      case L'%':
        if (options.literalstring[temp] != L'%') {
          clen = options.nlen;
          cset = options.num_charset;
        }
        break;
      case L'^':
        if (options.literalstring[temp] != L'^') {
          clen = options.slen;
          cset = options.sym_charset;
        }
        break;
      default: /* constant part of pattern */
        break;
    }
    if (cset) {
      for (index1 = 0; index1 < clen && cset[index1] != endstring[temp];)
        index1++;
      if (index1 == clen)
        index1 = clen - 1;
      for (index2 = 0; index2 < clen && cset[index2] != startstring[temp];)
        index2++;
      if (index2 == clen)
        index2 = 0;

      total_strings = (total_strings - 1) * clen + (unsigned long long)index1 - (unsigned long long)index2 + 1;
    }
  }

  return total_strings;
}

/* calculate the number of lines and bytes to output */
static void count_strings(unsigned long long *lines, unsigned long long *bytes, const options_type options) {
const wchar_t *startstring = options.startstring, *endstring = options.endstring;
size_t min = options.min, max = options.max;
size_t i, j;
unsigned long long temp;
int check_dupes; /* need to take duplicates into account */
unsigned long long extra_unicode_bytes = 0;

/* parameters for counting taking dupes into consideration */
size_t first, last; /* index of the first character and last character */
int start_point = 0, end_point = 0; /* bool; whether to consider a starting or ending string */

  if (max == 0) {
    *lines = 0;
    *bytes = 0;
    return;
  }

  if (output_unicode)
    suppress_finalsize = 1;

  /* Notes on unicode addition:
    If unicode output is enabled, *bytes will be wrong in here because
    it doesn't take into account the extra width of the unicode chars.
    But maybe we can easily calculate how many extra bytes will result
    and turn the suppress_finalsize flag back off so user gets the real size.
    We can only do this (with this approach anyway) if duplicates are not a
    factor, and if start/end strings are not specified.  So you will see this
    similar calculation done each time after the "normal" size is calculated.
    I have attempted to leave everything else as it was.  -jC */

  /* Check this just once up here so we don't have to clutter up stuff below -jC */
  if (options.pattern == NULL) {
    for (i = 0; i < max; i++) {
      if (options.pattern_info[i].is_fixed!=0 || options.pattern_info[i].cset==NULL || options.pattern_info[i].clen==0) {
        fprintf(stderr,"Error: pattern is unexpectedly NULL in unicode size calc sanity check\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  if (min == max) { /* strings of the same length */
    check_dupes = 0;
    for (i = 0; i < min; i++)
      if (options.pattern_info[i].duplicates < min)
        check_dupes = 1;
    if (check_dupes) { /* must take duplicates into account */
      first = 0;
      last = options.pattern_info[0].clen - 1;
      if (startstring) {
        first = options.pattern_info[0].start_index;
        start_point = 1;
      }
      if (endstring) {
        last = options.pattern_info[0].end_index;
        end_point = 1;
      }
      temp = calculate_with_dupes(start_point, end_point, first, last, 0, 0, 1, NULL, options, min);
    }
    else {
      temp = calculate_with_pattern(options.min_string, options.max_string, options);
    }

    if ((linecount > 0) && (options.endstring==NULL)) {
      *lines = linecount;
      *bytes = linecount * (max + 1);
    }
    else {
      *lines = temp;
      *bytes = temp * (max + 1);
    }

    extra_unicode_bytes = 0;

    if (output_unicode!=0 && check_dupes==0 && options.startstring==NULL && options.endstring==NULL) {
      for (i = 0; i < max; i++) {
        if (options.pattern_info[i].is_fixed!=0 || options.pattern_info[i].cset==NULL || options.pattern_info[i].clen==0)
          extra_unicode_bytes += (getmblen(options.pattern[i])-1)*(*lines);
        else
          extra_unicode_bytes += (wcstombs(NULL,options.pattern_info[i].cset,0)-options.pattern_info[i].clen)*(*lines)/options.pattern_info[i].clen;
      }
      *bytes += extra_unicode_bytes;

      /* Got the exact size in bytes so we can turn this off */
      suppress_finalsize = 0;
    }
  }
  else {
    /* add beginning from startstring to last_min */
    check_dupes = 0;
    for (i = 0; i < min; i++)
      if (options.pattern_info[i].duplicates < min)
        check_dupes = 1;
    if (check_dupes) { /* must take duplicates into account */
      first = 0;
      last = options.pattern_info[0].clen - 1;
      if (startstring) {
        first = options.pattern_info[0].start_index;
        start_point = 1;
      }
      temp = calculate_with_dupes(start_point, 0, first, last, 0, 0, 1, NULL, options, min);
    }
    else {
      if (startstring)
        temp = calculate_simple(startstring, options.last_min, options.low_charset, options.clen);
      else
        temp = (unsigned long long)pow((double)options.clen, (double)min);
    }

    *lines = temp;
    *bytes = temp * (min + 1);

    /* Extra unicode calc for what he calls the "beginning" (len=1->min) */
    if (output_unicode!=0 && check_dupes==0 && options.startstring==NULL && options.endstring==NULL) {
      extra_unicode_bytes = 0;
      for (i = 0; i < min; i++) {
        if (options.pattern_info[i].is_fixed!=0 || options.pattern_info[i].cset==NULL || options.pattern_info[i].clen==0)
          extra_unicode_bytes += (getmblen(options.pattern[i])-1)*(*lines);
        else
          extra_unicode_bytes += (wcstombs(NULL,options.pattern_info[i].cset,0)-options.pattern_info[i].clen)*(*lines)/options.pattern_info[i].clen;
      }
      *bytes += extra_unicode_bytes;
      suppress_finalsize = 0;
    }
    /* From here on out we will only do that similar calc
      if suppress_finalsize was already false, and actually
      we'll set it to true if we encounter anything we can't handle.*/

    /* add middle permutations */
    for (i = min + 1; i < max; i++) {
      /* i is 1 past the array index, careful here -jC */
      first = 0;
      last = options.pattern_info[0].clen - 1;
      check_dupes = 0;
      for (j = 0; j < i; j++)
        if (options.pattern_info[j].duplicates < i)
          check_dupes = 1;
      if (check_dupes)  /* must take duplicates into account */
        temp = calculate_with_dupes(0, 0, first, last, 0, 0, 1, NULL, options, i);
      else
        temp = (unsigned long long)pow((double)options.clen, (double)i);

      *lines += temp;
      *bytes += temp * (i+1);

      if (output_unicode!=0) {
        if (suppress_finalsize==0 && check_dupes==0) {
          extra_unicode_bytes = 0;
          for (j = 0; j < i; j++) {
            if (options.pattern_info[j].is_fixed!=0 || options.pattern_info[j].cset==NULL || options.pattern_info[j].clen==0)
              extra_unicode_bytes += (getmblen(options.pattern[j])-1)*temp;
            else
              extra_unicode_bytes += (wcstombs(NULL,options.pattern_info[j].cset,0)-options.pattern_info[j].clen)*temp/options.pattern_info[j].clen;
          }
          *bytes += extra_unicode_bytes;
        }
        else
          suppress_finalsize = 1;
      }
    }

    /* add ending from first_max to endstring */
    check_dupes = 0;
    for (i = 0; i < max; i++)
      if (options.pattern_info[i].duplicates < max)
        check_dupes = 1;
    if (check_dupes) { /* must take duplicates into account */
      first = 0;
      last = options.pattern_info[0].clen - 1;
      if (endstring) {
        last = options.pattern_info[0].end_index;
        end_point = 1;
      }
      temp = calculate_with_dupes(0, end_point, first, last, 0, 0, 1, NULL, options, max);
    }
    else {
      if (endstring)
        temp = calculate_simple(options.first_max, endstring, options.low_charset, options.clen);
      else
        temp = (unsigned long long)pow((double)options.clen, (double)max);
    }

    if ((linecount > 0) && (options.endstring==NULL)) {
      *lines += linecount;
      *bytes += linecount * (max + 1);
    }
    else {
      *lines += temp;
      *bytes += temp * (max + 1);
    }

    if (output_unicode!=0) {
      if (suppress_finalsize==0 && check_dupes==0) {
        extra_unicode_bytes = 0;
        for (j = 0; j < max; j++) {
          if (options.pattern_info[j].is_fixed!=0 || options.pattern_info[j].cset==NULL || options.pattern_info[j].clen==0)
            extra_unicode_bytes += (getmblen(options.pattern[j])-1)*temp;
          else
            extra_unicode_bytes += (wcstombs(NULL,options.pattern_info[j].cset,0)-options.pattern_info[j].clen)*temp/options.pattern_info[j].clen;
        }
        *bytes += extra_unicode_bytes;
      }
      else
        suppress_finalsize = 1;
    }
  }
}

static int finished(const wchar_t *block, const options_type options) {
size_t i;
  if (options.pattern == NULL) {
    for (i = 0; i < wcslen(block); i++)
      if (inc[i] < options.clen-1)
        return 0;
  }
  else {
    for (i = 0; i < wcslen(block); i++) {
      if (((options.pattern[i] == L'@') && (inc[i] < options.clen-1) && (options.literalstring[i] != L'@')) || \
          ((options.pattern[i] == L',') && (inc[i] < options.ulen-1) && (options.literalstring[i] != L',')) || \
          ((options.pattern[i] == L'%') && (inc[i] < options.nlen-1) && (options.literalstring[i] != L'%')) || \
          ((options.pattern[i] == L'^') && (inc[i] < options.slen-1) && (options.literalstring[i] != L'^'))) {
        return 0;
      }
    }
  }
  return 1;
}

static int too_many_duplicates(const wchar_t *block, const options_type options) {
wchar_t current_char = L'\0';
size_t dupes_seen = 0;

  while (*block != L'\0') {
    if (*block == current_char) {
      dupes_seen += 1;
      /* check for overflow of duplicates */
      if (dupes_seen > options.duplicates[0]) {
        if (find_index(options.low_charset, options.clen, current_char) != NPOS)
          return 1;
      }
      if (dupes_seen > options.duplicates[1]) {
        if (find_index(options.upp_charset, options.ulen, current_char) != NPOS)
          return 1;
      }
      if (dupes_seen > options.duplicates[2]) {
        if (find_index(options.num_charset, options.nlen, current_char) != NPOS)
          return 1;
      }
      if (dupes_seen > options.duplicates[3]) {
        if (find_index(options.sym_charset, options.slen, current_char) != NPOS)
          return 1;
      }
    }
    else {
      current_char = *block;
      dupes_seen = 1;
    }
    block++;
  }
  return 0;
}

static void increment(wchar_t *block, const options_type options) {
size_t i, start, stop;
int step;
size_t blocklen = wcslen(block);
size_t violator_pos = NPOS;
int can_inc_before_violator = 0; /*bool*/
int reached_violator = 0; /*bool*/
wchar_t prev_char = L'\0';
size_t consecutive_dupes = 0;

const wchar_t *matching_set;
size_t mslen = 0;

  if ((options.low_charset == NULL) || (options.upp_charset == NULL) || (options.num_charset == NULL) || (options.sym_charset == NULL)) {
     fprintf(stderr,"increment: SOMETHING REALLY BAD HAPPENED\n");
     exit(EXIT_FAILURE);
  }

  /*
      Starting at the "beginning" of string (assuming !inverted),
      find first character which violates max duplicates
      which is NOT a fixed character.
      Be aware that block2 may contain e.g.
      "dddaaa" but if pattern is "ddd@@@" only the 'a's should
      can be examined.  This is necessary because permute may
      be in use and we obviously can't skip over combinations of duplicate
      characters which are used as placeholders for the permuted words.

      If we do find such a character we will be forcing an increment
      at that position (or in front of it) and zero everything behind it.

      There must be one incrementable before that position though,
      otherwise we are completely done and can ignore what we found above.
      If this check wasn't done we'd run through the end because
      finish() and everything else expects that last combination
      to be returned even if it has too many dupes.
  */

  if (inverted == 1) {
    start = blocklen-1;
    stop = (size_t)-1;
    step = -1;
  }
  else {
    start = 0;
    stop = blocklen;
    step = 1;
  }

  for (i = start; i != stop; i += step) {
    if (options.pattern == NULL || (wcschr(L"@,%^",options.pattern[i])!=NULL
        && options.pattern[i]!=options.literalstring[i])) {

      if (can_inc_before_violator==0 && inc[i] < options.pattern_info[i].clen-1)
        can_inc_before_violator = 1;

      if (block[i] == prev_char) {
        if (++consecutive_dupes > options.pattern_info[i].duplicates) {
          if (can_inc_before_violator==1)
            violator_pos = i; /* Found him.  Bad boy! */
          break;
        }
      }
      else {
        prev_char = block[i];
        consecutive_dupes = 1;
      }
    }
    else
      prev_char = L'\0';
  }

  /* increment from end of string */

  if (inverted == 0) {
    start = blocklen-1;
    stop = (size_t)-1;
    step = -1;
  }
  else {
    start = 0;
    stop = blocklen;
    step = 1;
  }

  if (violator_pos == NPOS)
    reached_violator = 1; /*already handled because wasn't there to begin with*/

  for (i = start; i != stop; i += step) {
    if (i==violator_pos)
      reached_violator = 1;

    if (options.pattern == NULL) {
      matching_set = options.low_charset;
      mslen = options.clen;
    }
    else {
      switch (options.pattern[i]) {
        case L'@':
          matching_set = options.low_charset;
          mslen = options.clen;
          break;
        case L',':
          matching_set = options.upp_charset;
          mslen = options.ulen;
          break;
        case L'%':
          matching_set = options.num_charset;
          mslen = options.nlen;
          break;
        case L'^':
          matching_set = options.sym_charset;
          mslen = options.slen;
          break;
        default:
          matching_set = NULL;
      }
    }
    if (matching_set == NULL || (options.pattern != NULL && options.literalstring[i] == options.pattern[i])) {
      /* options.pattern cannot be null here due to logic above at top of loop */
      block[i] = options.pattern[i]; /* add pattern letter to word */
    }
    else {
      if (inc[i] < mslen-1 && reached_violator==1) {
        block[i] = matching_set[++inc[i]];
        break;
      }
      else {
        block[i] = matching_set[0];
        inc[i] = 0;
      }
    }
  }
}

static void *PrintPercentage(void *threadarg) {
struct thread_data *threaddata;
unsigned long long linec;
unsigned long long counter;
unsigned long long calc = 0;

  threaddata = (struct thread_data *) threadarg;

  while (1 != 0) {
    (void)sleep(10);

    /* Progress calc now based on line count rather than bytes due to unicode issues */

    linec = threaddata->finallinecount;
    counter = threaddata->linetotal;

    if (linec)
      calc = 100L * counter / linec;

    if (calc < 100)
      fprintf(stderr,"\ncrunch: %3llu%% completed generating output\n", calc);
  }

  pthread_exit(NULL);

  return (void *)1;
}

static void renamefile(const size_t end, const char *fpath, const char *outputfilename, const char *compressalgo) {
FILE *optr;     /* ptr to START output file; will be renamed later */
char *newfile;  /* holds the new filename */
char *finalnewfile; /*final filename with escape characters */
char *comptype; /* build -t string for 7z */
char *compoutput; /* build archive string for 7z */
char *findit = NULL;   /* holds location of / character */
int status;     /* rename returns int */
char buff[512]; /* buffer to hold line from wordlist */
pid_t pid; /*  pid and pid return */

  errno=0;
  memset(buff,0,sizeof(buff));

  fprintf(stderr,"\ncrunch: %3d%% completed generating output\n", (int)(100L * my_thread.linetotal / my_thread.finallinecount));

  finalnewfile = calloc((end*3)+5+strlen(fpath), sizeof(char)); /* max length will be 3x outname */
  if (finalnewfile == NULL) {
    fprintf(stderr,"rename: can't allocate memory for finalnewfile\n");
    exit(EXIT_FAILURE);
  }

  newfile = calloc((end*3)+5+strlen(fpath), sizeof(char)); /* max length will be 3x outname */
  if (newfile == NULL) {
    fprintf(stderr,"rename: can't allocate memory for newfile\n");
    exit(EXIT_FAILURE);
  }

  if (compressalgo != NULL) {
    comptype = calloc(strlen(compressalgo)+3, sizeof(char)); /* -t bzip2 plus CR */
    if (newfile == NULL) {
      fprintf(stderr,"rename: can't allocate memory for comptype\n");
      exit(EXIT_FAILURE);
    }
  }

  compoutput = calloc((end*3)+5+strlen(fpath), sizeof(char)); /* max length will be 3x outname */
  if (newfile == NULL) {
    fprintf(stderr,"rename: can't allocate memory for compoutput\n");
    exit(EXIT_FAILURE);
  }

  if (strncmp(outputfilename, fpath, strlen(fpath)) != 0) {
    status = rename(fpath, outputfilename); /* rename from START to user specified name */
    if (status != 0) {
      fprintf(stderr,"Error renaming file.  Status1 = %d  Code = %d\n",status,errno);
      fprintf(stderr,"The problem is = %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  else {
    if ((optr = fopen(fpath,"r")) == NULL) {
      fprintf(stderr,"rename: File START could not be opened\n");
      exit(EXIT_FAILURE);
    }
    else {  /* file opened above now read first line */
      (void)fgets(buff, (int)sizeof(buff), optr);
      strncat(newfile, buff, strlen(buff)-1); /* get rid of CR */
      while (feof(optr) == 0) {
        (void)fgets(buff, (int)sizeof(buff), optr);
      } /* all of this just to get last line */
      if (fclose(optr) != 0) {
        fprintf(stderr,"rename: fclose returned error number = %d\n", errno);
        fprintf(stderr,"The problem is = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
    }
    strcat(newfile, "-"); /* build new filename */
    strncat(newfile, buff, strlen(buff)-1); /* get rid of CR */

    strncpy(finalnewfile,fpath,strlen(fpath)-5);

    findit = strstr(newfile,"/");
    if (findit == NULL) {
      strncat(finalnewfile,newfile,strlen(newfile));
      strncat(finalnewfile, ".txt", 4);
    }
    else {
      while ((findit = strchr(newfile, '/')) != NULL) {
        *findit++ = ' ';
      }
      strncat(finalnewfile,newfile,strlen(newfile));
      strncat(finalnewfile, ".txt", 4);
    }

    status = rename(fpath, finalnewfile); /* rename START to proper filename */
    if (status != 0) {
      fprintf(stderr,"Error renaming file.  Status2 = %d  Code = %d\n",status,errno);
      fprintf(stderr,"The problem is = %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if (compressalgo != NULL) {
    /*@-type@*/
    pid = fork();
    (void)wait(&status);

    /*@=type@*/
    if (pid == 0) {
      strncat(comptype,"-t", 2);
      strncat(comptype, compressalgo, strlen(compressalgo));

      if (strncmp(outputfilename, fpath, 5) == 0) {
        strcat(compoutput, finalnewfile);

        if (strncmp(compressalgo, "lzma", 4) == 0) {
          fprintf(stderr,"Beginning lzma compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", finalnewfile, NULL);
        }
        if (strncmp(compressalgo, "bzip2", 5) == 0) {
          fprintf(stderr,"Beginning bzip2 compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", finalnewfile, NULL);
        }
        if (strncmp(compressalgo, "gzip", 4) == 0) {
          fprintf(stderr,"Beginning gzip compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", finalnewfile, NULL);
        }
        if (strncmp(compressalgo, "7z", 2) == 0) {
          strcat(compoutput, ".7z");
          status=execlp("7z", "7z", "a", comptype, "-mx=9", compoutput, finalnewfile, NULL);
        }
      }
      else {
        strcat(compoutput, outputfilename);

        if (strncmp(compressalgo, "lzma", 4) == 0) {
          strcat(compoutput, ".lzma");
          fprintf(stderr,"Beginning lzma compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", outputfilename, NULL);
        }
        if (strncmp(compressalgo, "bzip2", 5) == 0) {
          fprintf(stderr,"Beginning bzip2 compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", outputfilename, NULL);
        }
        if (strncmp(compressalgo, "gzip", 4) == 0) {
          fprintf(stderr,"Beginning gzip compression.  Please wait.\n");
          status=execlp(compressalgo, compressalgo, "-9", "-f", "-v", outputfilename, NULL);
        }
        if (strncmp(compressalgo, "7z", 2) == 0) {
          strcat(compoutput, ".7z");
          status=execlp("7z", "7z", "a", comptype, "-mx=9", compoutput, outputfilename, NULL);
        }
      }

      if (status != 0) {
        fprintf(stderr,"Error compressing file.  Status = %d  Code = %d\n", status, errno);
        fprintf(stderr,"The problem is = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      if (strncmp(compressalgo, "lzma", 4) != 0) {
        if (strncmp(outputfilename, fpath, 5) == 0)
          status = remove(finalnewfile);
        else
          status = remove(outputfilename);

        if (status != 0) {
          fprintf(stderr,"Error deleting file.  Status = %d  Code = %d\n", status, errno);
          fprintf(stderr,"The problem is = %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
      }
    }
  }
  free(newfile);
  free(finalnewfile);
  free(compoutput);
  if (compressalgo != NULL)
    free(comptype);
}


static void printpermutepattern(const wchar_t *block2, const wchar_t *pattern, const wchar_t *literalstring, wchar_t **wordarray) {
size_t j, t;
char mb[MB_CUR_MAX+1];
int mblen = 0;

  for (t = 0, j = 0; t < wcslen(pattern); t++) {
    if ((pattern[t] == L'@') || (pattern[t] == L',') || (pattern[t] == L'%') ||  (pattern[t] == L'^')) {
      switch (pattern[t]) {
      case L'@':
        if (literalstring[t] == L'@') {
          fprintf(fptr, "@");
          break;
        }
        /*@fallthrough@*/
      case L',':
        if (literalstring[t] == L',') {
          fprintf(fptr, ",");
          break;
        }
        /*@fallthrough@*/
      case L'%':
        if (literalstring[t] == L'%') {
          fprintf(fptr, "%%");
          break;
        }
        /*@fallthrough@*/
      case L'^':
        if (literalstring[t] == L'^') {
          fprintf(fptr, "^");
          break;
        }
        /*@fallthrough@*/
      default:
        /*fprintf(fptr, "%c", block2[t]);*/
        if (!output_unicode)
          fprintf(fptr, "%c", (char)(block2[t]&0xFF));
        else {
          mblen = wctomb(mb,block2[t]);
          if (mblen == -1) {
            fprintf(stderr,"Error: wctomb failed for char U+%04lX\n",(unsigned long)block2[t]);
            exit(EXIT_FAILURE);
          }
          mb[mblen]='\0';
          fprintf(fptr, "%s", mb);

          if (mblen > 1) {
            /* bytecounter will be incremented by 1 below, so just do one less here*/
            my_thread.bytecounter += (mblen-1);
          }
        }
      }
      my_thread.bytecounter+=1;
    }
    else {
      (void)make_narrow_string(gconvbuffer,wordarray[j],gconvlen);
      fprintf(fptr, "%s", gconvbuffer);
      my_thread.bytecounter+=strlen(gconvbuffer);

      j++;
    }
  }
  fprintf(fptr, "\n");
  my_thread.bytecounter++;
  my_thread.linecounter++;
}

static void Permute(const char *fpath, const char *outputfilename, const char *compressalgo, wchar_t **wordarray, const options_type options, const size_t sizePerm, size_t unchanged) {
size_t outer = 0;
size_t inner = 0;
size_t wordlength = 0;
size_t t;
wchar_t *temp[1];
wchar_t *block2;      /* block is word being created */

  errno = 0;

  if (sizePerm > unchanged) {
    for(outer = unchanged; outer < sizePerm; outer++) {
      *temp = wordarray[outer];
      for(inner = outer; inner > unchanged; inner--) {
        wordarray[inner] = wordarray[inner - 1];
      }
      wordarray[unchanged] = *temp;
      Permute(fpath, outputfilename, compressalgo, wordarray, options, sizePerm, unchanged+1);

      for(inner = unchanged; inner < outer; inner++) {
        wordarray[inner] = wordarray[inner + 1];
      }
      wordarray[outer] = *temp;
    }
  }
  else {
    if (outputfilename == NULL) {
      if (options.pattern == NULL) {
        for (t = 0; t < sizePerm; t++) {
          (void)make_narrow_string(gconvbuffer,wordarray[t],gconvlen);
          fprintf(fptr, "%s", gconvbuffer);
        }
        fprintf(fptr, "\n");
      }
      else {
          block2 = calloc(options.plen+1,sizeof(wchar_t)); /* block can't be bigger than max size */
          if (block2 == NULL) {
            fprintf(stderr,"permute: can't allocate memory for block2\n");
            exit(EXIT_FAILURE);
          }

          for (t = 0; t < options.plen; t++) {
            switch (options.pattern[t]) {
              case L'@':
                block2[t] = options.low_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              case L',':
                block2[t] = options.upp_charset[0]; /* placeholder is set so add   character */
                inc[t] = 0;
                break;
              case L'%':
                block2[t] = options.num_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              case L'^':
                block2[t] = options.sym_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              default:
                block2[t] = ' '; /* add pattern letter to word */
            }
          } /* for t=0 */

          while (!finished(block2, options) && !ctrlbreak) {
              if (!too_many_duplicates(block2, options))
                printpermutepattern(block2, options.pattern, options.literalstring, wordarray);
              increment(block2, options);
          }
          if (!too_many_duplicates(block2, options))
            printpermutepattern(block2, options.pattern, options.literalstring, wordarray);
          free(block2);
      }
    }
    else {
      size_t outlen;

      if ((fptr = fopen(fpath,"a+")) == NULL) { /* append to file */
        fprintf(stderr,"permute: File START could not be opened\n");
        fprintf(stderr,"The problem is = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      else {
        for (t = 0; t < sizePerm; t++) {
          outlen = make_narrow_string(gconvbuffer,wordarray[t],gconvlen);
          wordlength += outlen;
        }
        wordlength++; /*for cr*/
        if (options.pattern == NULL) {
          if ((my_thread.linecounter <= (linecount-1)) && (my_thread.bytecounter <= (bytecount - wordlength))) { /* not time to create a new file */
            for (t = 0; t < sizePerm; t++) {
              (void)make_narrow_string(gconvbuffer,wordarray[t],gconvlen);
              fprintf(fptr, "%s", gconvbuffer);
            }
            my_thread.bytecounter+=wordlength;
            fprintf(fptr, "\n");
            my_thread.linecounter++;
          }
          else {
            my_thread.bytetotal+=my_thread.bytecounter;
            my_thread.linetotal+=my_thread.linecounter;

            if (fclose(fptr) != 0) {
              fprintf(stderr,"permute: fclose returned error number = %d\n", errno);
              fprintf(stderr,"The problem is = %s\n", strerror(errno));
              exit(EXIT_FAILURE);
            }

            renamefile(((wordlength*2)+5), fpath, outputfilename, compressalgo);
            if ((fptr = fopen(fpath, "w")) == NULL) {
              fprintf(stderr,"permute2: Ouput file START could not be opened\n");
              exit(EXIT_FAILURE);
            }
            my_thread.linecounter = 0;
            my_thread.bytecounter = 0;
            for (t = 0; t < sizePerm; t++) {
              (void)make_narrow_string(gconvbuffer,wordarray[t],gconvlen);
              fprintf(fptr, "%s", gconvbuffer);
            }
            my_thread.bytecounter+=wordlength;
            fprintf(fptr, "\n");
            my_thread.linecounter++;
          }
        }
        else { /* user specified a pattern */
          block2 = calloc(options.plen+1,sizeof(wchar_t)); /* block can't be bigger than max size */
          if (block2 == NULL) {
            fprintf(stderr,"permute2: can't allocate memory for block2\n");
            exit(EXIT_FAILURE);
          }

          for (t = 0; t < options.plen; t++) {
            switch (options.pattern[t]) {
              case L'@':
                block2[t] = options.low_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              case L',':
                block2[t] = options.upp_charset[0]; /* placeholder is set so add   character */
                inc[t] = 0;
                break;
              case L'%':
                block2[t] = options.num_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              case L'^':
                block2[t] = options.sym_charset[0]; /* placeholder is set so add character */
                inc[t] = 0;
                break;
              default:
                block2[t] = L' '; /* add pattern letter to word */
            }
          } /* for t=0 */

          while (!finished(block2, options) && !ctrlbreak) {
            if ((my_thread.linecounter <= (linecount-1)) && (my_thread.bytecounter <= (bytecount - wordlength))) { /* not time to create a new file */
              if (!too_many_duplicates(block2, options))
                printpermutepattern(block2, options.pattern, options.literalstring, wordarray);
              increment(block2, options);
            }
            else {
              my_thread.bytetotal+=my_thread.bytecounter;
              my_thread.linetotal+=my_thread.linecounter;

              if (fptr == NULL) {
                fprintf(stderr,"permute: something really bad happened!\n");
                exit(EXIT_FAILURE);
              }
              else {
                if (fclose(fptr) != 0) {
                  fprintf(stderr,"permute2: fclose returned error number = %d\n", errno);
                  fprintf(stderr,"The problem is = %s\n", strerror(errno));
                  exit(EXIT_FAILURE);
                }
              }

              renamefile(((wordlength*2)+5), fpath, outputfilename, compressalgo);
              if ((fptr = fopen(fpath, "w")) == NULL) {
                fprintf(stderr,"permute2: Ouput file START could not be opened\n");
                free(block2);
                exit(EXIT_FAILURE);
              }
              my_thread.linecounter = 0;
              my_thread.bytecounter = 0;
            }
          }
          if (!too_many_duplicates(block2, options))
            printpermutepattern(block2, options.pattern, options.literalstring, wordarray);
          free(block2);
        }

        if (fptr == NULL) {
          fprintf(stderr,"permute2: something really bad happened!\n");
          exit(EXIT_FAILURE);
        }
        else {
          if (ferror(fptr) != 0) {
            fprintf(stderr,"permute2: fprintf failed = %d\n", errno);
            fprintf(stderr,"The problem is = %s\n", strerror(errno));
            exit(EXIT_FAILURE);
          }
          if (fclose(fptr) != 0) {
            fprintf(stderr,"permute3: fclose returned error number = %d\n", errno);
            fprintf(stderr,"The problem is = %s\n", strerror(errno));
            exit(EXIT_FAILURE);
          }
        }
      }
    }
  }
}

static void Permutefilesize(wchar_t **wordarray, const size_t sizePerm, const size_t length, size_t unchanged) {
size_t outer = 0;
size_t inner = 0;
size_t t;
wchar_t *temp[1];

  if (sizePerm > unchanged) {
    for(outer = unchanged; outer < sizePerm; outer++) {
      *temp = wordarray[outer];
      for(inner = outer; inner > unchanged; inner--) {
        wordarray[inner] = wordarray[inner - 1];
      }
      wordarray[unchanged] = *temp;
      Permutefilesize(wordarray, sizePerm, length, unchanged+1);

      for(inner = unchanged; inner < outer; inner++) {
        wordarray[inner] = wordarray[inner + 1];
      }
      wordarray[outer] = *temp;
    }
  }
  else {
    for (t = 0; t < length; t++) {
      if (!output_unicode)
        my_thread.bytecounter += wcslen(wordarray[t]);
      else
        my_thread.bytecounter += wcstombs(NULL,wordarray[t],0);
    }
    my_thread.bytecounter++;
  }
}

static void loadstring(wchar_t *block2, const size_t j, const wchar_t *startblock, const options_type options) {
size_t k;

  if (startblock == NULL) { /* is startblock null? */
    if (options.pattern == NULL) { /* Yes so now test if pattern null? */
      block2[j] = options.low_charset[0]; /* pattern is null so add character */
      inc[j] = 0;
    }
    else { /* pattern is not null */
      switch (options.pattern[j]) {
        case L'@':
          if (options.literalstring[j] != L'@') {
            block2[j] = options.low_charset[0]; /* placeholder is set so add character */
            inc[j] = 0;
          }
          else {
           block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
          break;
        case L',':
          if (options.literalstring[j] != L',') {
            block2[j] = options.upp_charset[0]; /* placeholder is set so add character */
            inc[j] = 0;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
          break;
        case L'%':
          if (options.literalstring[j] != L'%') {
            block2[j] = options.num_charset[0]; /* placeholder is set so add character */
            inc[j] = 0;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
          break;
        case L'^':
          if (options.literalstring[j] != L'^') {
            block2[j] = options.sym_charset[0]; /* placeholder is set so add character */
            inc[j] = 0;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
          break;
        default:
          block2[j] = options.pattern[j]; /* add pattern letter to word */
        }
      }
    }
  else { /* startblock is not null */
    if (options.pattern == NULL) { /* Yes so now test if pattern null? */
      block2[j] = startblock[j]; /* pattern is null so add character */
      for(k = 0; k < wcslen(options.low_charset); k++) {
        if (block2[j] == options.low_charset[k]) {
          inc[j] = k;
        }
      }
    }
    else {
      switch (options.pattern[j]) {
        case L'@':
          if (options.literalstring[j] != L'@') {
            block2[j] = startblock[j]; /* pattern is null so add character */
            for(k = 0; k < wcslen(options.low_charset); k++)
              if (block2[j] == options.low_charset[k])
                inc[j] = k;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
        break;
        case L',':
          if (options.literalstring[j] != L',') {
            block2[j] = startblock[j]; /* pattern is null so add character */
            for(k = 0; k < wcslen(options.upp_charset); k++)
              if (block2[j] == options.upp_charset[k])
                inc[j] = k;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
        break;
        case L'%':
          if (options.literalstring[j] != L'%') {
            block2[j] = startblock[j]; /* pattern is null so add character */
            for(k = 0; k < wcslen(options.num_charset); k++)
              if (block2[j] == options.num_charset[k])
                inc[j] = k;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
        break;
        case L'^':
          if (options.literalstring[j] != L'^') {
            block2[j] = startblock[j]; /* pattern is null so add character */
            for(k = 0; k < wcslen(options.sym_charset); k++)
              if (block2[j] == options.sym_charset[k])
                inc[j] = k;
          }
          else {
            block2[j] = options.pattern[j]; /* add pattern letter to word */
          }
        break;
        default:
          block2[j] = options.pattern[j]; /* add pattern letter to word */
      }
    }
  }
}

static void chunk(const size_t start, const size_t end, const wchar_t *startblock, const options_type options, const char *fpath, const char *outputfilename, const char *compressalgo) {
size_t i,j;      /* loop counters */
wchar_t *block2;      /* block is word being created */
size_t outlen; /* temp for size of narrow output string */

  errno = 0;
  block2 = calloc(end+1,sizeof(wchar_t)); /* block can't be bigger than max size */
  if (block2 == NULL) {
    fprintf(stderr,"chunk: can't allocate memory for block2\n");
    exit(EXIT_FAILURE);
  }

  for (i = start; (i <= end) && !ctrlbreak; i++) {
    for (j = 0; j < i; j++) {
      loadstring(block2, j, startblock, options);
    }
    startblock = NULL;

    if (outputfilename == NULL) { /* user wants to display words on screen */
      if (options.endstring == NULL) {
        while ((!finished(block2,options) && !ctrlbreak) && (my_thread.linecounter < (linecount-1))) {
          if (!too_many_duplicates(block2, options)) {
            (void)make_narrow_string(gconvbuffer,block2,gconvlen);
            fprintf(fptr, "%s\n", gconvbuffer);
            my_thread.linecounter++;
          }
          increment(block2, options);
        }
        if (!too_many_duplicates(block2, options)) { /*flush last word */
          (void)make_narrow_string(gconvbuffer,block2,gconvlen);
          fprintf(fptr, "%s\n", gconvbuffer);
        }
        if (my_thread.linecounter == (linecount-1)) {
          goto killloop;
        }
      }
      else {
        while (!finished(block2,options) && !ctrlbreak && (wcsncmp(block2,options.endstring,wcslen(options.endstring)) != 0) ) {
          if (!too_many_duplicates(block2, options)) {
            (void)make_narrow_string(gconvbuffer,block2,gconvlen);
            fprintf(fptr, "%s\n", gconvbuffer);
            my_thread.linecounter++;
          }
          increment(block2, options);
        }
        if (!too_many_duplicates(block2, options)) { /*flush last word */
          (void)make_narrow_string(gconvbuffer,block2,gconvlen);
          fprintf(fptr, "%s\n", gconvbuffer);
        }
        if (wcsncmp(block2,options.endstring,wcslen(options.endstring)) == 0)
          break;
      }
    }
    else { /* user wants to generate a file */
      if ((fptr = fopen(fpath,"a+")) == NULL) { /* append to file */
        fprintf(stderr,"chunk1: File START could not be opened\n");
        fprintf(stderr,"The problem is = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      else { /* file opened start writing.  file will be renamed when done */
        while (!finished(block2, options) && (ferror(fptr) == 0) && !ctrlbreak) {
          if ((options.endstring != NULL) && (wcsncmp(block2,options.endstring,wcslen(options.endstring)) == 0))
            break;

          outlen = make_narrow_string(gconvbuffer,block2,gconvlen);

          if ((my_thread.linecounter <= (linecount-1)) && (my_thread.bytecounter <= (bytecount - outlen))) { /* not time to create a new file */
            if (!too_many_duplicates(block2, options)) {
              fprintf(fptr, "%s\n", gconvbuffer);
              if (ferror(fptr) != 0) {
                fprintf(stderr,"chunk1: fprintf failed = %d\n", errno);
                fprintf(stderr,"The problem is = %s\n", strerror(errno));
                free(block2);
                exit(EXIT_FAILURE);
              }
              my_thread.bytecounter += (unsigned long long)outlen+1;
              my_thread.linecounter++;
              my_thread.linetotal++;
            }
            increment(block2,options);
          }
          else { /* time to create a new file */
            my_thread.bytetotal+=my_thread.bytecounter;

            if (fclose(fptr) != 0) {
              fprintf(stderr,"chunk1: fclose returned error number = %d\n",errno);
              fprintf(stderr,"The problem is = %s\n", strerror(errno));
              free(block2);
              exit(EXIT_FAILURE);
            }
            renamefile(end, fpath, outputfilename, compressalgo);

            if ((options.endstring == NULL) || (wcsncmp(block2,options.endstring,wcslen(options.endstring)) != 0)) {
              if ((fptr = fopen(fpath, "w")) == NULL) {
                fprintf(stderr,"chunk2: Ouput file START could not be opened\n");
                free(block2);
                exit(EXIT_FAILURE);
              }
              my_thread.linecounter = 0;
              my_thread.bytecounter = 0;
            }
            else {
              goto killloop;
            }
          }
        }

        if (fptr == NULL) {
          fprintf(stderr,"chunk: something really bad happened\n");
          exit(EXIT_FAILURE);
        }
        else {
          if (!too_many_duplicates(block2, options)) {

            outlen = make_narrow_string(gconvbuffer,block2,gconvlen);
            fprintf(fptr, "%s\n", gconvbuffer); /* flush the last word */

            my_thread.linecounter++;
            my_thread.linetotal++;
            my_thread.bytecounter += (unsigned long long)outlen+1;

            if (ferror(fptr) != 0) {
              fprintf(stderr,"chunk2: fprintf failed = %d\n", errno);
              fprintf(stderr,"The problem is = %s\n", strerror(errno));
              free(block2);
              exit(EXIT_FAILURE);
            }
          }
          if (fclose(fptr) != 0) {
            fprintf(stderr,"chunk2: fclose returned error number = %d\n", errno);
            fprintf(stderr,"The problem is = %s\n", strerror(errno));
            free(block2);
            exit(EXIT_FAILURE);
          }

          if ((options.endstring != NULL) && (wcsncmp(block2,options.endstring,wcslen(options.endstring)) == 0))
            break;

          if (ctrlbreak == 1)
            break;
        }
      } /* else from if fopen */
    } /* else from outputfilename == NULL */
  } /* for start < end loop */

  if (ctrlbreak == 1 ) {
    (void)make_narrow_string(gconvbuffer,block2,gconvlen);
    fprintf(stderr,"Crunch ending at %s\n",gconvbuffer);
  }

  killloop:
  my_thread.bytetotal += my_thread.bytecounter;

  if ((outputfilename != NULL) && !ctrlbreak) {
    renamefile(end, fpath, outputfilename, compressalgo);
  }

  free(block2);
}

static void usage() {
  fprintf(stderr,"crunch version %s\n\n", version);
  fprintf(stderr,"Crunch can create a wordlist based on criteria you specify.  The outout from crunch can be sent to the screen, file, or to another program.\n\n");
  fprintf(stderr,"Usage: crunch <min> <max> [options]\n");
  fprintf(stderr,"where min and max are numbers\n\n");
  fprintf(stderr,"Please refer to the man page for instructions and examples on how to use crunch.\n");
}

static wchar_t *resumesession(const char *fpath, const wchar_t *charset) {
FILE *optr;     /* ptr to START output file; will be renamed later */
char buff[512]; /* buffer to hold line from wordlist */
wchar_t *startblock;
size_t j, k;

  errno = 0;
  memset(buff, 0, sizeof(buff));

  if ((optr = fopen(fpath,"r")) == NULL) {
    fprintf(stderr,"resume: File START could not be opened\n");
    exit(EXIT_FAILURE);
  }
  else {  /* file opened above now read first line */
    while (feof(optr) == 0) {
      (void)fgets(buff, (int)sizeof(buff), optr);
      ++my_thread.linecounter;
      my_thread.bytecounter += (unsigned long long)strlen(buff);
    } /* all of this just to get last line */
    my_thread.linecounter--; /* -1 to get correct num */
    my_thread.bytecounter -= (unsigned long long)strlen(buff);

    if (fclose(optr) != 0) {
      fprintf(stderr,"resume: fclose returned error number = %d\n", errno);
      fprintf(stderr,"The problem is = %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (buff[0])
      buff[strlen(buff)-1]='\0';

    startblock = alloc_wide_string(buff,NULL);

    fprintf(stderr,"Resuming from = %s\n", buff);

    for (j = 0; j < wcslen(startblock); j++) {
      for(k = 0; k < wcslen(charset); k++)
        if (startblock[j] == charset[k])
          inc[j] = k;
    }
    return startblock;
  }
}

/*@null@*/ static wchar_t *readcharsetfile(const char *charfilename, const char *charsetname, int* r_is_unicode) {
FILE *optr;  /* ptr to user specified charset file */
char *temp;  /* holds character set name from charset file */
char *chars; /* holds characters from specified charsetname i.e. stuff after [ */
char *charset = NULL; /* holds the characters to use (narrow) */
wchar_t* wcharset = NULL; /* wide version of charset, this is the return */
char buff[512]; /* buffer to hold line from charset file */

  errno = 0;
  memset(buff, 0, sizeof(buff));
  if ((optr = fopen(charfilename,"r")) == NULL) { /* open file to read from */
    fprintf(stderr,"readcharset: File %s could not be opened\n", charfilename);
    fprintf(stderr,"The problem is = %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  else {  /* file opened above now scan file for charsetname */
    while (fgets(buff, (int)sizeof(buff), optr) != NULL) {
      temp = strtok(buff, " ");
      if (strcmp(charsetname, temp)==0) {
        chars = strtok(NULL, "["); /* move to = sign in file */
        if ((chars = strtok(NULL, "\n")) != NULL) { /* get rest of string after [ */
          charset = calloc(strlen(chars), sizeof(char));
          if (charset == NULL) {
            fprintf(stderr,"readcharset: can't allocate memory for charset\n");
            exit(EXIT_FAILURE);
          }
          if (strncmp(&chars[(strlen(chars)-1)], "]", 1) == 0)
            memcpy(charset, chars, strlen(chars)-1); /* don't strip off space only ]*/
          else
            memcpy(charset, chars, strlen(chars)-2); /* strip off ] */

          wcharset = alloc_wide_string(charset, r_is_unicode);
          free(charset);
        }
        break;
      }
    }
    if (fclose(optr) != 0) {
      fprintf(stderr,"readcharset: fclose returned error number = %d\n", errno);
      fprintf(stderr,"The problem is = %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return wcharset;
}

static wchar_t **readpermute(const char *charfilename, int* r_is_unicode) {
FILE *optr;  /* ptr to user specified charset file */
wchar_t **wordarray2; /* holds characters from specified charsetname */
/*@notnull@*/ char buff[512]; /* buffer to hold line from charset file */
size_t temp = 0;

  errno = 0;
  numofelements = 0;
  memset(buff, 0, sizeof(buff));
  if ((optr = fopen(charfilename,"r")) == NULL) { /* open file to read from */
    fprintf(stderr,"readpermute: File %s could not be opened\n", charfilename);
    fprintf(stderr,"The problem is = %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  else {  /* file opened above now get the number of line in file */
    while (fgets(buff, (int)sizeof(buff), optr) != NULL) {
      if (buff[0] != '\n')
        numofelements++;
    }
    (void)fseek(optr, 0, SEEK_SET);

    wordarray2 = calloc(numofelements, sizeof(wchar_t*));
    if (wordarray2 == NULL) {
      fprintf(stderr,"readpermute: can't allocate memory for wordarray1\n");
      exit(EXIT_FAILURE);
    }
    while (fgets(buff, (int)sizeof(buff), optr) != NULL) {
      if (buff[0] != '\n' && buff[0]!='\0') {
        buff[strlen(buff)-1]='\0';
        wordarray2[temp++] = alloc_wide_string(buff,r_is_unicode);
      }
    }

    if (fclose(optr) != 0) {
      fprintf(stderr,"readpermute: fclose returned error number = %d\n", errno);
      fprintf(stderr,"The problem is = %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  return wordarray2;
  }
}

static void copy_without_dupes(wchar_t* dest, wchar_t* src) {
  size_t i;
  size_t len = wcslen(src);

  dest[0]=L'\0';

  for (i = 0; i < len; ++i) {
    if (wcschr(dest,src[i])==NULL)
      wcsncat(dest,&src[i],1);
  }
}
