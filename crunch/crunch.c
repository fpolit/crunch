/*  character-set based brute-forcing algorithm
 *  Copyright (C) 2004 by mimayin@aciiid.ath.cx
 *  Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013 by bofh28@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 only of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  crunch version 1.0 was written by mimayin@aciiid.ath.cx
 *  all later versions of crunch have been updated by bofh28@gmail.com
 *
 *  Changelog:
 *  version 1.0 initial release in 2004 (guess based on copyright date) 194 lines of code
 *  version 1.1 added support to read character list from a specified file
 *  version 1.2 added output parameter
 *  version 1.3 added -c parameter to break apart output file. I would prefer to break
 *                 apart on file size instead of number of lines, but there is no reliable
 *                 way to get that information while the file is being created.
 *  version 1.4 added better help documentation and fixed some bugs:
 *              space character was sometimes getting stripped off in charset
 *              replace if (pattern[0]==0) { with if ((strncmp(&pattern[0],"\0",1)==0)) {
 *                 to make splint happy
 *              rename templ variable to pattern
 *              changed some fixed size local buffers to dynamically allocated buffers
 *              if max-len was larger than strlen of the -t parameter crunch would generate
 *                 duplicate words
 *              document some of the code
 *  version 1.5 fixed length and -s parameter.  Before if you did ./crunch 1 3 -s 	
 *                 you would only get j-z and not j-zzz
 *              converted some fixed length buffers to variable length
 *              added checks to fclose
 *              fixed it so that -s and -t can now work together
 *              added support to generate wordlists greater than 2GB even though several
 *                 programs can't access files of this size (john the ripper for example)
 *              This support works on Ubuntu intrepid.  This probably works on most Linux
 *                 platforms.  Support for Solaris, HP-UX, Windows, BSD is unknown.
 *  version 1.6 add option (-p) to support factorial instead of exponentail processing.  i.e.
 *                 if you did ./crunch 3 3 abc you would get 27 results (3x3x3): a through ccc
 *                 This new option will give you 6 results (3! = 3x2x1)
 *                 i.e. (abc, acb, bac, bca, cab, cba)
 *                 the permute function was written by Richard Heathfield
 *                 and copied from
 *                 http://bytes.com/groups/c/536779-richard-heathfields-tsp-permutation-algorithm
 *                 I did change temp from an int to char to make splint happy.
 *                 Richard Heathfield may be contacted at rjh@cpax.org.uk
 *  version 1.7 add option (-m) to support factoring words instead of characters
 *  version 1.8 add option (-z) to support compressing the output
 *  version 1.9 only need permute2 (remove permute and renamed permute2 to permute)
 *              fix off by 1 error - reported by MikeCa
 *              fix issue where output file wasn't being renamed
 *              fork progress process
 *              really fix it so that -s and -t can now work together
 *              when using -c sometimes the output was 1 larger than requested
 *              cstring_cmp from http://www.anyexample.com/programming/c/qsort__sorting_array_of_strings__integers_and_structs.xml to sort words to permute so the results will be in sorted order
 *  version 2.0 permute now supports -c -o & -z. -f is also supported for permuting letters
 *              really really fix it so that -s and -t can now work together
 *              added null checks for parameter values to prevent segmentation faults
 *              added support to invert chunk output - written by Pillenski
 *              added support to breakup file based on size - written by Pillenski
 *  version 2.1 permute now supports -b
 *              add signal handling - pressing ctrl break will cause crunch
 *                 to finish the word it is processing and then gracefully exit instead
 *                 of leaving files open and words half finished.  This is the first
 *                 step in supporting resume.
 *              chunk now supports resume
 *  version 2.2 pattern supports numbers and symbols
 *  version 2.3 fix bytecount
 *              add new options to usage
 *  version 2.4 fix usage (-m and not -n) - reported by Tape
 *              clarified -b option in help and man file - reported by Tape
 *              fix Makefile install to /pentest/passwords not /pentest/password -
 *                 reported by Tape
 *              make bytecount behave consistently - reported by Tape
 *              fixed up copyrights
 *  version 2.5 -q permute supports reading from strings from file
 *              sorted parameters in code and usage
 *              -t supports upper and lower case letters
 *              -f supports multiple charsets
 *              add -v option to show version information
 *              add correct gpl.txt (version 2 not 3)
 *              fix charset.lst (some symbol14 had 15 chars)
 *              add symbol14, symbol14-space, symbol-all, and symbol-all-space to charset.lst
 *              removed examples and parameters from usage.  I added a sentence
 *                 telling people to refer to the man page.  It is easier to update
 *                 the man only.
 *              combined -m and -p into -p
 *              got rid of some unnecessary casts
 *              changed all mallocs to callocs, this shuts up a few splint warnings
 *  version 2.6 fix -p seg fault - reported by Tape
 *              improve -p documentation - reported by Tape
 *              fix Makefile to install to correct directory again - reported by Tape
 *              fix Makefile to install charset.lst - reported by Tape
 *              fix memory leak
 *              replace if (argv[i+1] != NULL) with if (i+1 < argc) as argv[i+1]
 *                 can be filled garbage so it is not an accurate test
 *              fix an off by 1 in resume counter
 *              resume now respects the -b parameter, previously it was ignored
 *              -s now supports the @*%^ symbols in -t
 *              added status report when saving to file
 *              renamed some variables to better names
 *              added a few comments to variables
 *              added a hex string 0123456789abcdef to charset.lst
 *  version 2.7 fix progress bar when using -s
 *              fixed typo man file - Thanks Tape
 *              add -u option to suppress all filesize and linecount so crunch
 *                 can feed another program
 *              fork a child process for progress %%
 *              Niclas Kroon added swedish characters to the charset.lst
 *              permute supports -t
 *              added -std=c99 to Makefile since unsigned long long isn't in c89
 *              ran valgrind and fixed a small memory issue.  I forgot to allocate
 *                 space for the \0 character in a few places.  Doh!
 *              improved documentation of the charset field
 *  version 2.8 fix progress message.  It could cause a fatal error under certain
 *                 circumstances
 *  version 2.9 fix divide by zero error.
 *  version 3.0 fix wrong version number
 *              changed the * character of -t option to a , (comma) as * is a reserved character
 *              strip off blank lines when reading file for permute (-q option)
 *              I fixed a problem with using -i and -t together
 *              add -l to allow for literal characters of @,%^
 *              fix -b and -c and % output
 *              fix permute -t to work with -b and -c
 *              fixed crash when / character was in filename
 *                 replace / with space - reported by Nazagul
 *  version 3.0.1 fix printpermutepattern - it was using $ instead of ,
 *  version 3.1 make -l take a string so you can have @ and characters
 *              add TB and PB to output size
 *              fix comments referencing $ that should be ,
 *              add -e to end generation after user specified string (useful
 *                 when piping crunch to a program)
 *  version 3.2 add -d to limit duplicate characters
 *              put correct function name into error messages to help with debugging
 *              fix Makefile uninstall to remove crunch directory and install GPL.TXT
 *              removed flag5 as it wasn't needed
 *              if you press Ctrl-C crunch will now print where it stops so you
 *                 can resume piping crunch into another program
 *  version 3.3 add more information to help section
 *              Fixed mem leaks, invalid comparisons - fixed by JasonC
 *              Error messages and startup summary now go to stderr (-u now 
 *                 unnecessary) - fixed by JasonC
 *              Fixed startup delay due to long sequences of dupe-skipped 
 *                 strings - fixed by JasonC
 *              Added unicode support - written by JasonC
 *              fix write and compress error - reported and fixed by amontero
 *              fix printpercentage -> linecounter should be ->linetotal
 *              add support for 7z
 *  version 3.4 fix -e problem reported by hajjid
 *              test compile using Ubuntu 12.10 and fixed the following issues:
 *                 reorder flags in Makefile so crunch can compile successfully
 *                 remove finall variable from printpercentage
 *                 remove loaded from main
 *  version 3.5 make changes to the man based on suggestions from Jari Aalto
 *              pass pidret to void to make warning go away
 *              rename GPL.TXT to COPYING
 *              removed need for -o to use -c i.e. you can use -c any time now
 *              fixed resume
 *              fixed linecount and size when using -c and/or -e
 *  version 3.6 fix endstring problem reported and fixed by mr.atreat@gmail.com
 *              fix a memory allocation reported and fixed by Hxcan Cai
 *              allow Makefile to detect Darwin so make will work properly reported and fixed by Marcin
 *
 *  TODO: Listed in no particular order
 *         add resume support to permute (I am not sure this is possible)
 *         make permute more intelligent (min, max) (I am not sure this is possible either)
 *         support SIGINFO when Linux supports it, use SIGUSR1 until SIGINFO is available
 *         finalbytecount isn't currently correct for unicode chars unless -p used
 *         let user specify placeholder characters (@,%^)
 *         add date support?
 *         specify multiple charset names using -f i.e. -f charset.lst + ualpha 123 +
 *         make permute use -e
 *         revamp compression part of renamefile 7z doesn't delete original file
 *         size calculations are wrong when min or max is larger than 12
 *         write word to temp file for resuming after power outage
 *
 *  usage: ./crunch <min-len> <max-len> [charset]
 *  e.g: ./crunch 3 7 abcdef
 *
 *  This example will compute all passwords between 3 and 7 chars
 *  using 'abcdef' as the character set and dump it to stdout.
 *
 *  usage: ./crunch <from-len> <to-len> [-f <path to charset.lst> charset-name] [-o wordlist.txt or START] [-t [FIXED]@@@@] [-s startblock]
 *
 *  Options:
 *  -b          : maximum bytes to write to output file. depending on the blocksize
 *                files may be some bytes smaller than specified but never bigger.
 *  -c          : numbers of lines to write to output file, only works if "-o START"
 *                is used, eg: 60  The output files will be in the format of starting
 *                letter - ending letter for example:
 *                crunch 1 5 -f /pentest/password/charset.lst mixalpha -o START -c 52
 *                will result in 2 files: a-7.txt and 8-\ .txt  The reason for the
 *                slash in the second filename is the ending character is space and
 *                ls has to escape it to print it.  Yes you will need to put in
 *                the \ when specifying the filename.
 *  -d          : specify -d [n][@,%^] to suppress generation of strings with more
 *                than [n] adjacent duplicates from the given character set. For example:
 *                ./crunch 5 5 -d 2@
 *                Will print all combinations with 2 or less adjacent lowercase duplicates.
 *  -e          : tells crunch to stop generating words at string.  Useful when piping
 *                crunch to another program.
 *  -f          : path to a file containing a list of character sets, eg: charset.lst
 *                name of the character set in the above file eg:
 *                mixalpha-numeric-all-space
 *  -i          : inverts the output so the first character will change very often
 *  -l          : literal characters to use in -t @,%^
 *  -o          : allows you to specify the file to write the output to, eg:
 *                wordlist.txt
 *  -p          : prints permutations without repeating characters.  This option
 *                CANNOT be used with -s.  It also ignores min and max lengths.
 *  -q          : Like the -p option except it reads the strings from the specified
 *                file.  It CANNOT be used with -s.  It also ignores min and max.
 *  -r          : resume a previous session.  You must use the same command line as
 *                the previous session.
 *  -s          : allows you to specify the starting string, eg: 03god22fs
 *  -t [FIXED]@,%^  : allows you to specify a pattern, eg: @@god@@@@
 *                where the only the @'s will change with lowercase letters
 *                the ,'s will change with uppercase letters
 *                the %'s will change with numbers
 *                the ^'s will change with symbols
 *  -u          : The -u option disables the printpercentage thread.  This should be the last option.
 *  -z          : adds support to compress the generated output.  Must be used
 *                with -o option.  Only supports gzip, bzip, lzma, and 7z.
 *
 *  This code can be easily adapted for use in brute-force attacks
 *  against network services or cryptography.
 *
 *  Compiles on: linux (32 and 64 bit Ubuntu for sure, 32 and 64 bit Linux in
 *     general works.  I have received word that crunch compiles on MacOS.
 *     Juan reports Freebsd x64, i386 and OSX Mountain Lion compiles and works
 *     perfectly. It should compile on the other Unix and Linux OSs but I don't
 *     don't have access to any of the those systems.  Please let me know.
 */

#include "utils.h"

//int main(int argc, char **argv) {
int crunch(){
  size_t flag = 0;   /* 0 for chunk 1 for permute */
  size_t flag3 = 0;  /* 0 display file size info 1 supress file size info */
  size_t flag4 = 0;  /* 0 don't create thread 1 create print % done thread */
  size_t resume = 0; /* 0 new session 1 for resume */
  size_t arglen = 0; /* used in -b option to hold strlen */
  size_t min, max;   /* minimum and maximum size */
  size_t temp;       /* temp varible */
  size_t templen;    /* another temp var */

  unsigned long calc = 0;  /* recommend count */
  size_t dupvalue; /* value for duplicates option */

  int i = 3;      /* minimum number of parameters */
  int ret = 0;    /* return value of pthread_create */
  int multi = 0;

  int saw_unicode_input = 0;

  wchar_t *charset; /* character set */
  wchar_t *upp_charset = NULL;
  wchar_t  *num_charset = NULL;
  wchar_t *sym_charset = NULL;
  wchar_t *startblock = NULL; /* user specified starting point */
  wchar_t *pattern = NULL;    /* user specified pattern */
  char *fpath = NULL;          /* path to outputfilename if specified*/
  char *outputfilename = NULL; /* user specified filename to write output to */
  char *compressalgo = NULL;   /* user specified compression program */
  wchar_t *endstring = NULL;      /* hold -e option */
  char *charsetfilename = NULL;
  char *tempfilename = NULL;
  char *bcountval = NULL;
  wchar_t *literalstring = NULL; /* user passed something using -l */
  char *hold;
  char *endptr; /* temp pointer for duplicates option */

  wchar_t *tempwcs; /* temp used while converting input */

  options_type options; /* store validated parameters passed to the program */

  wchar_t **wordarray = NULL; /* array to store words */

  pthread_t threads;

  (void) signal(SIGINT, ex_program);
//  (void) signal(SIGINFO, printme);

  memset(&threads,0,sizeof(threads));
  fptr = stdout;

  if (setlocale(LC_ALL, "")==NULL) {
    fprintf(stderr,"Error: setlocale() failed\n");
    exit(EXIT_FAILURE);
  }

  options.low_charset = options.upp_charset = options.num_charset = options.sym_charset = NULL;
  options.pattern = options.literalstring = NULL;
  options.startstring = options.endstring = NULL;
  options.last_min = options.first_max = NULL;
  options.min_string = options.max_string = NULL;
  options.pattern_info = NULL;

  if ((argc == 2) && (strncmp(argv[1], "-v", 2) == 0)) {  /* print version information */
    fprintf(stderr,"crunch version %s\n", version);
    return 0;
  }

  if (argc < 3) {
    usage();
    return 0;
  }

  gconvlen = MAXSTRING*MB_CUR_MAX+1;
  gconvbuffer = (char*)malloc(gconvlen);
  if (!gconvbuffer) {
    fprintf(stderr,"Error: Failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }

  charset = dupwcs(def_low_charset);
  if (charset == NULL) {
    fprintf(stderr,"crunch: can't allocate memory for default charset\n");
    exit(EXIT_FAILURE);
  }

  upp_charset = dupwcs(def_upp_charset);
  if (upp_charset == NULL) {
    fprintf(stderr,"crunch: can't allocate memory for default upp_charset\n");
    exit(EXIT_FAILURE);
  }

  num_charset = dupwcs(def_num_charset);
  if (num_charset == NULL) {
    fprintf(stderr,"crunch: can't allocate memory for default num_charset\n");
    exit(EXIT_FAILURE);
  }

  sym_charset = dupwcs(def_sym_charset);
  if (sym_charset == NULL) {
    fprintf(stderr,"crunch: can't allocate memory for default sym_charset\n");
    exit(EXIT_FAILURE);
  }

  my_thread.finallinecount = my_thread.linecounter = my_thread.linetotal = 0;

  if (argc >= 4) {
    if ((argc > i) && (*argv[i] != '-')) { /*test for ./crunch 1 2 -? */
      if (*argv[i] != '+') {
        /* user specified charset */
        free(charset);

        templen = strlen(argv[i])+1;
        tempwcs = (wchar_t*)malloc(templen*sizeof(wchar_t));
        charset = (wchar_t*)malloc(templen*sizeof(wchar_t));
        if (tempwcs==NULL || charset==NULL) {
          fprintf(stderr,"crunch: can't allocate memory for user charset\n");
          exit(EXIT_FAILURE);
        }

        (void)make_wide_string(tempwcs,argv[i],templen,&saw_unicode_input);
        copy_without_dupes(charset, tempwcs);
        free(tempwcs);
      }
      i++; /* user specified a charset and a parameter */
    }

    if ((argc > i) && (*argv[i] != '-')) { /*test for ./crunch 1 2 -? */
      if (*argv[i] != '+') {
        /* user specified upp_charset */
        free(upp_charset);

        templen = strlen(argv[i])+1;
        tempwcs = (wchar_t*)malloc(templen*sizeof(wchar_t));
        upp_charset = (wchar_t*)malloc(templen*sizeof(wchar_t));

        if (tempwcs==NULL || upp_charset==NULL) {
          fprintf(stderr,"crunch: can't allocate memory for upp_charset\n");
          exit(EXIT_FAILURE);
        }

        (void)make_wide_string(tempwcs,argv[i],templen,&saw_unicode_input);
        copy_without_dupes(upp_charset, tempwcs);
        free(tempwcs);
      }
      i++; /* user specified a upp_charset and a parameter */
    }
    if ((argc > i) && (*argv[i] != '-')) { /*test for ./crunch 1 2 -? */
      if (*argv[i] != '+') {
        /* user specified num_charset */
        free(num_charset);

        templen = strlen(argv[i])+1;
        tempwcs = (wchar_t*)malloc(templen*sizeof(wchar_t));
        num_charset = (wchar_t*)malloc(templen*sizeof(wchar_t));

        if (tempwcs==NULL || num_charset==NULL) {
          fprintf(stderr,"crunch: can't allocate memory for num_charset\n");
          exit(EXIT_FAILURE);
        }

        (void)make_wide_string(tempwcs,argv[i],templen,&saw_unicode_input);
        copy_without_dupes(num_charset, tempwcs);
        free(tempwcs);
      }
      i++; /* user specified a num_charset and a parameter */
    }
    if ((argc > i) && (*argv[i] != '-')) { /*test for ./crunch 1 2 -? */
      if (*argv[i] != '+') {
        /* user specified sym_charset */
        free(sym_charset);

        templen = strlen(argv[i])+1;
        tempwcs = (wchar_t*)malloc(templen*sizeof(wchar_t));
        sym_charset = (wchar_t*)malloc(templen*sizeof(wchar_t));

        if (tempwcs==NULL || sym_charset==NULL) {
          fprintf(stderr,"crunch: can't allocate memory for sym_charset\n");
          exit(EXIT_FAILURE);
        }

        (void)make_wide_string(tempwcs,argv[i],templen,&saw_unicode_input);
        copy_without_dupes(sym_charset, tempwcs);
        free(tempwcs);
      }
      i++; /* user specified a sym_charset and a parameter */
    }
  }

  min = (size_t)atoi(argv[1]);
  max = (size_t)atoi(argv[2]);

  if (max < min) {
    fprintf(stderr,"Starting length is greater than the ending length\n");
    exit(EXIT_FAILURE);
  }

  if (max > MAXSTRING) {
    fprintf(stderr,"Crunch can only make words with a length of less than %d characters\n", MAXSTRING+1);
    exit(EXIT_FAILURE);
  }

  for (temp = 0; temp < 4; temp++)
    options.duplicates[temp] = (size_t)-1;

  for (; i<argc; i+=2) { /* add 2 to skip the parameter value */
    if (strncmp(argv[i], "-b", 2) == 0) { /* user wants to split files by size */
      if (i+1 < argc) {
        bcountval = argv[i+1];
        if (bcountval != NULL) {
          arglen = strlen(bcountval);

          for (temp = 0; temp < arglen; temp++)
            bcountval[temp] = tolower(bcountval[temp]);

          if (strstr(bcountval, "kb") != 0) multi=1000;
          else if (strstr(bcountval, "mb")  != 0) multi = 1000000;
          else if (strstr(bcountval, "gb")  != 0) multi = 1000000000;
          else if (strstr(bcountval, "kib") != 0) multi = 1024;
          else if (strstr(bcountval, "mib") != 0) multi = 1048576;
          else if (strstr(bcountval, "gib") != 0) multi = 1073741824;

          calc = strtoul(bcountval, NULL, 10);
          bytecount = calc * multi;
          if (calc > 4 && multi >= 1073741824 && bytecount <= 4294967295ULL) {
            fprintf(stderr,"ERROR: Your system is unable to process numbers greater than 4.294.967.295. Please specify a filesize <= 4GiB.\n");
            exit(EXIT_FAILURE);
          }
        }
        else {
          fprintf(stderr,"bvalue has a serious problem\n");
          exit(EXIT_FAILURE);
        }
      }
      else {
        fprintf(stderr,"Please specify a value\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-c", 2) == 0) {
      if (i+1 < argc) {
        linecount = strtoul(argv[i+1], NULL, 10);
        if ((linecount*max) > 2147483648UL) {
          calc = (2147483648UL/(unsigned long)max);
          fprintf(stderr,"WARNING: resulting file will probably be larger than 2GB \n");
          fprintf(stderr,"Some applications (john the ripper) can't use wordlists greater than 2GB\n");
          fprintf(stderr,"A value of %lu ",calc);
          fprintf(stderr,"or less should result in a file less than 2GB\n");
          fprintf(stderr,"The above value is calcualated based on 2147483648UL/max\n");
          exit(EXIT_FAILURE);
        }
      }
      else {
        fprintf(stderr,"Please specify the number of lines you want\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-d", 2) == 0) {  /* specify duplicates to skip */
      if (i+1 < argc) {
        dupvalue = (size_t)strtoul(argv[i+1], &endptr, 10);
        if (argv[i+1] == NULL || endptr == argv[i+1]) {
          fprintf(stderr,"-d must be followed by [n][@,%%^]\n");
          exit(EXIT_FAILURE);
        }
        if (*endptr == '\0') /* default to @ charset */
          options.duplicates[0] = dupvalue;
        while (*endptr != '\0') {
          switch (*endptr) {
            case '@':
              options.duplicates[0] = dupvalue;
              break;
            case ',':
              options.duplicates[1] = dupvalue;
              break;
            case '%':
              options.duplicates[2] = dupvalue;
              break;
            case '^':
              options.duplicates[3] = dupvalue;
              break;
            default:
              fprintf(stderr,"the type of duplicates must be one of [@,%%^]\n");
              exit(EXIT_FAILURE);
              /*@notreached@*/
              break;
          }
          endptr++;
        }
      }
      else {
        fprintf(stderr,"Please specify the type of duplicates to skip\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-e", 2) == 0) {
      if (i+1 < argc) {
        templen = strlen(argv[i+1])+1;
        endstring = (wchar_t*)malloc(templen*sizeof(wchar_t));
        if (endstring == NULL) {
          fprintf(stderr,"Error: Can;t allocate mem for endstring\n");
          exit(EXIT_FAILURE);
        }
        (void)make_wide_string(endstring, argv[i+1], templen, &saw_unicode_input);
      }
      else {
        fprintf(stderr,"Please specify the string you want crunch to stop at\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-f", 2) == 0) { /* user specified a charset.lst */
      if (i+1 < argc) {
        charsetfilename = calloc(strlen(argv[i+1])+1, sizeof(char));
        if (charsetfilename == NULL) {
          fprintf(stderr,"can't allocate memory for charsetfilename\n");
          exit(EXIT_FAILURE);
        }
        memcpy(charsetfilename, argv[i+1], strlen(argv[i+1]));
        i += 2; /* skip -f and filename */
      }

      if ((i < argc) && (argv[i] != NULL) && (*argv[i] != '-')) { /* lowercase */
        free(charset);
        charset = readcharsetfile(charsetfilename, argv[i], &saw_unicode_input);
        if (charset == NULL) {
          fprintf(stderr,"charset = %s not found in %s\n", argv[i], charsetfilename);
          exit(EXIT_FAILURE);
        }
        else {
          numofelements = wcslen(charset);
          wordarray = calloc(numofelements, sizeof(wchar_t*));
          if (wordarray == NULL) {
            fprintf(stderr,"can't allocate memory for wordarray1\n");
            exit(EXIT_FAILURE);
          }
          for (temp = 0; temp < numofelements; temp++) {
            wordarray[temp] = calloc(2, sizeof(wchar_t)); /* space for letter + \0 */
            if (wordarray[temp] == NULL) {
              fprintf(stderr,"can't allocate memory for wordarray2\n");
              exit(EXIT_FAILURE);
            }
            wordarray[temp][0] = charset[temp];
            wordarray[temp][1] = L'\0';
          }
          i++;
        }

/* uppercase */
        if (i < argc && argv[i]!=NULL && *argv[i] != '-') {
          if (*argv[i] != '+') {
            free(upp_charset);
            upp_charset = readcharsetfile(charsetfilename, argv[i], &saw_unicode_input);
            if (upp_charset == NULL) {
              fprintf(stderr,"upp_charset = %s not found in %s\n", argv[i], charsetfilename);
              exit(EXIT_FAILURE);
            }
          }
          i++;
        }
/* numbers */
        if (i < argc && argv[i]!=NULL && *argv[i] != '-') {
          if (*argv[i] != '+') {
            free(num_charset);
            num_charset = readcharsetfile(charsetfilename, argv[i], &saw_unicode_input);
            if (num_charset == NULL) {
              fprintf(stderr,"num_charset = %s not found in %s\n", argv[i], charsetfilename);
              exit(EXIT_FAILURE);
            }
          }
          i++;
        }
/* symbols */
        if (i < argc && argv[i]!=NULL && *argv[i] != '-') {
          if (*argv[i] != '+') {
            free(sym_charset);
            sym_charset = readcharsetfile(charsetfilename, argv[i], &saw_unicode_input);
            if (sym_charset == NULL) {
              fprintf(stderr,"sym_charset = %s not found in %s\n", argv[i], charsetfilename);
              exit(EXIT_FAILURE);
            }
          }
        }
        i -= 2; /* have to subtract 2 to continue processing parameter values */
      }
      else {
        fprintf(stderr,"Please specify a filename and character set\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-i", 2) == 0) { /* user wants to invert output calculation */
      inverted = 1;
      i--; /* decrease by 1 since -i has no parameter value */
    }

    if (strncmp(argv[i], "-l", 2) == 0) { /* user wants to list literal characters */
      if (i+1 < argc) {
        literalstring = alloc_wide_string(argv[i+1],&saw_unicode_input);
      }
      else {
        fprintf(stderr,"Please specify a list of characters you want to treat as literal @?%%^\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-o", 2) == 0) {  /* outputfilename specified */
      flag4=1;
      if (i+1 < argc) {
        hold = strrchr(argv[i+1], '/');
        outputfilename = argv[i+1];
        if (hold == NULL) {
          fpath = calloc(6, sizeof(char));
          if (fpath == NULL) {
            fprintf(stderr,"crunch: can't allocate memory for fpath1\n");
            exit(EXIT_FAILURE);
          }
          memcpy(fpath, "START", 5);
          tempfilename = outputfilename;
        }
        else {
          temp = strlen(argv[i+1])-strlen(hold)+1;
          tempfilename = &argv[i+1][temp];
          fpath = calloc(temp+6, sizeof(char)); /* 6 for START +1 */
          if (fpath == NULL) {
            fprintf(stderr,"crunch: can't allocate memory for fpath2\n");
            exit(EXIT_FAILURE);
          }
          memcpy(fpath, argv[i+1], temp);
          strncat(fpath, "START", 5);
        }
      }
      else {
        fprintf(stderr,"Please specify a output filename\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-p", 2) == 0) { /* user specified letters/words to permute */
      if (i+1 < argc) {
        flag = 1;
        numofelements = (size_t)(argc-i)-1;

        if (numofelements == 1) {
          tempwcs = alloc_wide_string(argv[i+1],&saw_unicode_input);
          numofelements = wcslen(tempwcs);

          wordarray = calloc(numofelements, sizeof(wchar_t*));
          if (wordarray == NULL) {
            fprintf(stderr,"can't allocate memory for wordarray3\n");
            exit(EXIT_FAILURE);
          }

          for (temp = 0; temp < numofelements; temp++) {
            wordarray[temp] = calloc(2, sizeof(wchar_t)); /* space for letter + \0 char */
            if (wordarray[temp] == NULL) {
              fprintf(stderr,"can't allocate memory for wordarray2\n");
              exit(EXIT_FAILURE);
            }
            wordarray[temp][0] = tempwcs[temp];
            wordarray[temp][1] = '\0';
          }

          free(tempwcs);
        }
        else {
          wordarray = calloc(numofelements, sizeof(wchar_t*));
          if (wordarray == NULL) {
            fprintf(stderr,"can't allocate memory for wordarray3\n");
            exit(EXIT_FAILURE);
          }

          for (temp = 0; temp < numofelements; temp++, i++) {
            wordarray[temp] = alloc_wide_string(argv[i+1],&saw_unicode_input);
          }
        }
        /* sort wordarray so the results are sorted */
        qsort(wordarray, temp, sizeof(char *), wcstring_cmp);
      }
      else {
        fprintf(stderr,"Please specify a word or words to permute\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-q", 2) == 0) { /* user specified file of words to permute */
      if (i+1 < argc) {
        wordarray = readpermute(argv[i+1],&saw_unicode_input);
        /* sort wordarray so the results are sorted */
        qsort(wordarray, numofelements, sizeof(char *), wcstring_cmp);
        flag = 1;
      }
      else {
        fprintf(stderr,"Please specify a filename for permute to read\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-r", 2) == 0) { /* user wants to resume a previous session */
      resume = 1;
      i--; /* decrease by 1 since -r has no parameter value */
    }

    if (strncmp(argv[i], "-s", 2) == 0) { /* startblock specified */
      if (i+1 < argc && argv[i+1]) {
        startblock = alloc_wide_string(argv[i+1],&saw_unicode_input);
        if (wcslen(startblock) != min) {
          fprintf(stderr,"Warning: minimum length should be %d\n", (int)wcslen(startblock));
          exit(EXIT_FAILURE);
        }
      }
      else {
        fprintf(stderr,"Please specify the word you wish to start at\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-t", 2) == 0) { /* pattern specified */
      if (i+1 < argc) {
        pattern = alloc_wide_string(argv[i+1],&saw_unicode_input);

        if ((max > wcslen(pattern)) || (min < wcslen(pattern))) {
          fprintf(stderr,"The maximum and minimum length should be the same size as the pattern you specified. \n");
          fprintf(stderr,"min = %d  max = %d  strlen(%s)=%d\n",(int)min, (int)max, argv[i+1], (int)wcslen(pattern));
          exit(EXIT_FAILURE);
        }
      }
      else {
        fprintf(stderr,"Please specify a pattern\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strncmp(argv[i], "-u", 2) == 0) {  /* suppress filesize info */
      fprintf(stderr,"Disabling printpercentage thread.  NOTE: MUST be last option\n\n");
      flag4=0;
      i--;
    }

    if (strncmp(argv[i], "-z", 2) == 0) {  /* compression algorithm specified */
      if (i+1 < argc) {
        compressalgo = argv[i+1];
        if ((compressalgo != NULL) && (strcmp(compressalgo, "gzip") != 0) && (strcmp(compressalgo, "bzip2") != 0) && (strcmp(compressalgo,"lzma") != 0) && (strcmp(compressalgo,"7z") != 0)) {
          fprintf(stderr,"Only gzip, bzip2, lzma, and 7z are supported\n");
          exit(EXIT_FAILURE);
        }
      }
      else {
        fprintf(stderr,"Only gzip, bzip2, lzma, and 7z are supported\n");
        exit(EXIT_FAILURE);
      }
    }
  } /* end parameter processing */

  /* parameter validation */
  if ((literalstring != NULL) && (pattern == NULL)) {
    fprintf(stderr,"you must specify -t when using -l\n");
    exit(EXIT_FAILURE);
  }

  if ((literalstring != NULL) && (pattern != NULL)) {
    if (wcslen(literalstring) != wcslen(pattern)) {
      fprintf(stderr,"Length of literal string should be the same length as pattern^\n");
      exit(EXIT_FAILURE);
    }
  }

  if (tempfilename != NULL) {
    if ((bytecount > 0) && (strcmp(tempfilename, "START") != 0)) {
      fprintf(stderr,"you must use -o START if you specify a count\n");
      exit(EXIT_FAILURE);
    }
  }

  if (endstring != NULL) {
    if (max != wcslen(endstring)) {
      fprintf(stderr,"End string length must equal maximum string size\n");
      exit(EXIT_FAILURE);
    }
  }

  if (startblock != NULL && endstring != NULL) {
    for (temp = 0; temp < wcslen(startblock); temp++) {

/*Added by mr.atreat@gmail.com
  The previous endstring function was broken because it used the ASCII/unicode
  values to check if endstring was greater than startblock. This revision uses
  the actual input charset character order to determine which is greater. */
      wchar_t startcharsrch = startblock[temp];
      wchar_t * startpos;
      startpos = wcschr(charset, startcharsrch);
      int startplace = startpos - charset;

      wchar_t endcharsrch = endstring[temp];
      wchar_t * endpos;
      endpos = wcschr(charset, endcharsrch);
      int endplace = endpos - charset;

      if (startplace > endplace) {
        fprintf(stderr,"End string must be greater than start string\n");
        exit(EXIT_FAILURE);
      }
      if (startplace < endplace)
	break;

/*Removed by mr.atreat@gmail.com
     if (startblock[temp] > endstring[temp]) {
        fprintf(stderr,"End string must be greater than start string\n");
        exit(EXIT_FAILURE);
      }
      if (startblock[temp] < endstring[temp])
       exit(EXIT_FAILURE);
       break;
*/
    }
  }

  if (literalstring == NULL) { /* create a default literalstring */
    size_t sizectr;
    literalstring = calloc(max+1, sizeof(wchar_t));
    if (literalstring == NULL) {
      fprintf(stderr,"can't allocate memory for literalstring\n");
      exit(EXIT_FAILURE);
    }
    for (sizectr = 0; sizectr < max; ++sizectr)
      literalstring[sizectr] = L'-';
    literalstring[max] = L'\0';
  }

  options.low_charset = charset;
  options.upp_charset = upp_charset;
  options.num_charset = num_charset;
  options.sym_charset = sym_charset;
  options.clen = charset ? wcslen(charset) : 0;
  options.ulen = upp_charset ? wcslen(upp_charset) : 0;
  options.nlen = num_charset ? wcslen(num_charset) : 0;
  options.slen = sym_charset ? wcslen(sym_charset) : 0;
  options.pattern = pattern;
  options.plen = pattern ? wcslen(pattern) : 0;
  options.literalstring = literalstring;
  options.endstring = endstring;
  options.max = max;

  if (pattern != NULL && startblock != NULL)
      if (!check_member(startblock, &options)) {
        fprintf(stderr,"startblock is not valid according to the pattern/literalstring\n");
        exit(EXIT_FAILURE);
      }
  if (pattern != NULL && endstring != NULL)
      if (!check_member(endstring, &options)) {
        fprintf(stderr,"endstring is not valid according to the pattern/literalstring\n");
        exit(EXIT_FAILURE);
      }

  if (endstring && too_many_duplicates(endstring, options)) {
    fprintf(stderr,"Error: End string set by -e will never occur (too many duplicate chars)\n");
    exit(EXIT_FAILURE);
  }

  if (saw_unicode_input) {
    char response[8];
    fprintf(stderr,
      "Notice: Detected unicode characters.  If you are piping crunch output\n"\
      "to another program such as john or aircrack please make sure that program\n"\
      "can handle unicode input.\n"\
      "\n");

    fprintf(stderr,"Do you want to continue? [Y/n] ");

    fgets(response,8,stdin);
    if (toupper(response[0])=='N') {
      fprintf(stderr,"Aborted by user.\n");
      exit(EXIT_FAILURE);
    }

    output_unicode = 1;
  }

  /* start processing */
  options.startstring = startblock;
  options.min = min;
  fill_minmax_strings(&options);
  fill_pattern_info(&options);

  if (resume == 1) {
    if (startblock != NULL) {
      fprintf(stderr,"you cannot specify a startblock and resume\n");
      exit(EXIT_FAILURE);
    }
    if (flag == 0) {
      startblock = resumesession(fpath, charset);
      min = wcslen(startblock);
      increment(startblock, options);
    }
    if (flag == 1) {
      fprintf(stderr,"permute doesn't support resume\n");
      exit(EXIT_FAILURE);
    }
  }
  else {
    if (fpath != NULL)
      (void)remove(fpath);
  }

  if (flag == 0) { /* chunk */
    count_strings(&my_thread.linecounter, &my_thread.finalfilesize, options);

    /* subtract already calculated data size */
    my_thread.finallinecount = my_thread.linecounter - my_thread.linetotal;
    my_thread.finalfilesize -= my_thread.bytetotal;

    /* Appears that linetotal can never be nonzero there,
      so why are we subtracting it?  -jC */


    /* if ((linecount > 0) && (finallinecount > linecount)) {
      finallinecount = linecount - linetotal;
    */ /* TODO: must calculate finalfilesize during line calculation */

    if (flag3 == 0) {
      if (suppress_finalsize == 0) {
        fprintf(stderr,"Crunch will now generate the following amount of data: ");
        fprintf(stderr,"%llu bytes\n",my_thread.finalfilesize);
        fprintf(stderr,"%llu MB\n",my_thread.finalfilesize/1048576);
        fprintf(stderr,"%llu GB\n",my_thread.finalfilesize/1073741824);
        fprintf(stderr,"%llu TB\n",my_thread.finalfilesize/1099511627776);
        fprintf(stderr,"%llu PB\n",my_thread.finalfilesize/1125899906842624);
      }
      fprintf(stderr,"Crunch will now generate the following number of lines: ");


      /* I don't see any case where finallinecount is not set there
          which would require the finalfilesize/(max+1) calc.
          I'm assuming this was a leftover from previous versions.
          At any rate, finallinecount must now always be correct or else
          other stuff will break.  So if something's wrong with line
          count output, don't code around it here like it was. -jC */

      /*
      if (pattern == NULL)
        fprintf(stderr,"%llu \n",my_thread.finallinecount);
      else
        fprintf(stderr,"%llu \n",my_thread.finalfilesize/(max+1));
      */

      fprintf(stderr,"%llu \n",my_thread.finallinecount);

      (void) sleep(3);
      if (flag4 == 1) {
        /* valgrind may report a memory leak here, because the progress report
          thread never actually terminates cleanly.  No big whoop. */
        ret = pthread_create(&threads, NULL, PrintPercentage, (void *)&my_thread);
        if (ret != 0){
          fprintf(stderr,"pthread_create error is %d\n", ret);
          exit(EXIT_FAILURE);
        }
        (void) pthread_detach(threads);
      }
    }
    my_thread.finalfilesize+=my_thread.bytetotal;
    my_thread.linecounter = 0;

    chunk(min, max, startblock, options, fpath, outputfilename, compressalgo);
  }
  else { /* permute */
    my_thread.finallinecount = 1;
    temp = 1;
    min = 0;

    /* calculate number of lines per section */
    while (temp <= numofelements) {
      my_thread.finallinecount *= temp;
      temp++;
    }
    my_thread.linecounter=my_thread.finallinecount; /* hold factoral */

    if (pattern == NULL) {
      if (wordarray == NULL) {
        fprintf(stderr,"Internal error: wordarray is NULL (it shouldn't be)\n");
        exit(EXIT_FAILURE);
      }

      my_thread.finalfilesize = 1;
      for (temp = 0; temp <numofelements; temp++) {
        templen = make_narrow_string(gconvbuffer,wordarray[temp],gconvlen);
        my_thread.finalfilesize += templen;
      }
      my_thread.finalfilesize *= my_thread.linecounter;
    }
    else {
      unsigned long long extra_unicode_bytes = 0;

      for (temp = 0; temp < wcslen(pattern); temp++) {
        switch (pattern[temp]) {
        case L'@':
          if (literalstring[temp] != L'@') {
            my_thread.finallinecount *= wcslen(charset);
          }
          break;
        case L',':
          if (literalstring[temp] != L',') {
            my_thread.finallinecount *= wcslen(upp_charset);
          }
          break;
        case L'%':
          if (literalstring[temp] != L'%') {
            my_thread.finallinecount *= wcslen(num_charset);
          }
          break;
        case L'^':
          if (literalstring[temp] != L'^') {
            my_thread.finallinecount *= wcslen(sym_charset);
          }
          break;
        default:
          min++;
          break;
        }
      }

      if (output_unicode != 0 && my_thread.finallinecount!=0) {
        /* Need one more pass to calculate how many extra bytes this will generate.
          Can't be in the first loop because we need the total product.  */

        for (temp = 0; temp < wcslen(pattern); temp++) {
          switch (pattern[temp]) {
          case L'@':
            if (literalstring[temp] != L'@') {
              extra_unicode_bytes += (wcstombs(NULL,charset,0)-wcslen(charset))*my_thread.finallinecount/wcslen(charset);
            }
            break;
          case L',':
            if (literalstring[temp] != L',') {
              extra_unicode_bytes += (wcstombs(NULL,upp_charset,0)-wcslen(upp_charset))*my_thread.finallinecount/wcslen(upp_charset);
            }
            break;
          case L'%':
            if (literalstring[temp] != L'%') {
              extra_unicode_bytes += (wcstombs(NULL,num_charset,0)-wcslen(num_charset))*my_thread.finallinecount/wcslen(num_charset);
            }
            break;
          case L'^':
            if (literalstring[temp] != L'^') {
              extra_unicode_bytes += (wcstombs(NULL,sym_charset,0)-wcslen(sym_charset))*my_thread.finallinecount/wcslen(sym_charset);
            }
            break;
          default:
            break;
          }
        }
      }

      /* calculate filesize */
      Permutefilesize(wordarray, numofelements, min, 0);

      my_thread.finalfilesize = (my_thread.finallinecount/my_thread.linecounter) * (my_thread.linecounter*(max-min)+my_thread.bytecounter);
      my_thread.finalfilesize += extra_unicode_bytes;
    }

    if (flag3 == 0) {
      if (suppress_finalsize == 0) {
        fprintf(stderr,"Crunch will now generate approximately the following amount of data: ");
        fprintf(stderr,"%llu bytes\n",my_thread.finalfilesize);
        fprintf(stderr,"%llu MB\n",my_thread.finalfilesize/1048576);
        fprintf(stderr,"%llu GB\n",my_thread.finalfilesize/1073741824);
        fprintf(stderr,"%llu TB\n",my_thread.finalfilesize/1099511627776);
        fprintf(stderr,"%llu PB\n",my_thread.finalfilesize/1125899906842624);
      }
      fprintf(stderr,"Crunch will now generate the following number of lines: ");
      fprintf(stderr,"%llu \n",my_thread.finallinecount);
      (void) sleep(3);
    }

    my_thread.bytecounter = 0;
    my_thread.linecounter=0;

    Permute(fpath, outputfilename, compressalgo, wordarray, options, numofelements, 0);

    my_thread.bytetotal+=my_thread.bytecounter;
    my_thread.linetotal+=my_thread.linecounter;
    if ((outputfilename != NULL) && (my_thread.linecounter != 0))
      renamefile(((size_t)(my_thread.finallinecount*2)+5), fpath, outputfilename, compressalgo);
  }

  if (wordarray) {
    for (temp = 0; temp < numofelements; temp++)
      free(wordarray[temp]);
    free(wordarray);
  }

  free(gconvbuffer);

  free(fpath);
  free(charsetfilename);
  free(charset);
  free(upp_charset);
  free(num_charset);
  free(sym_charset);

  free(pattern);
  free(startblock);
  free(endstring);
  free(literalstring);

  free(options.last_min);
  free(options.first_max);
  free(options.min_string);
  free(options.max_string);
  free(options.pattern_info);

  return 0;
}
