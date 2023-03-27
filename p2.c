// Devin Cline
// Professor Carroll
// EC Deadline November 27, 2021

#include "p2.h"

// notes: CHK is macro used to check if return values are errors and handle printing error messages

char buff[MAX_CHARS];       // buffer for storing contents of input. used with buffPtr in parse to call getword
char *newargv[MAX_ITEMS];   // populated in parse. stores commands to call execvp with
int newargvIndex = 0;       // tracks position/number of entries in newargv
int oFlags = 0;             // flags for opening file
char *lessFlag = NULL;      // flag for detecting <, stores address of redirect
char *greaterFlag = NULL;   // flag for detecting > or >! that also stores address of redirect
int outputOverwrite = 0;    // bool flag for detecting greaterExclamation. if >!, greaterFlag stores address and outputOverwrite is set to 1
pid_t childPid;             // used to track parent / child from fork
int pipeStarts[10];         // tracks where in newargv the commands are for pipes
int i_pipeStarts = 0;       // tracks index in pipeStarts
int numPipes = 0;           // tracks how many pipes are on a line
int ampersandFlag = 0;      // boolean flag for background process &
extern int openSingleQuote; // flag for unmatched single quote used in getword.c as well
int openSingleQuote = 0;

// output: 0 if no flag present, 1 if any flag present
// purpose: indicate whether a metacharacter has been detected, flag been set
int flagPresent()
{
  return lessFlag || greaterFlag || numPipes || ampersandFlag;
}

// input: array of file descriptors to be closed, number of file descriptors used
// purpose: close all possible open files
// notes: this is a simple process that will do an unnecessary amount of closes sometimes but will not cause issues by doing so
void closeFileDescriptors(int *filedes, int numFDs)
{
  int i;
  for (i = 0; i < numFDs; i++)
    close(filedes[i]);
}

// purpose: handle logic if pipeFlag has been set
// note: this is entered after the fork. only child enters and then forks grandchild within.
//       this is because of vertical piping.
void pipeHelper()
{
  int filedes[20];           // hard coded for max number of pipes 10 (2 file descriptors per pipe)
  int numFDs = 2 * numPipes; // number of file descriptors in line
  pid_t middleChild = -1;    // middle children are repeated if multiple pipes. if one pipe, middleChild is actually the "last child"
  pid_t lastChild = -1;
  int input_fd = NULL;
  int output_fd = NULL;
  int i; // used for tracking loop for forking middle children

  i_pipeStarts = numPipes - 2;
  // notes for i_pipeStarts: the second to last value in the array is where the first of the middle children will start looking for exec values.
  //                         the commands were populated in pipeStarts from the start of the array as they appeared on the line so it's in reverse order for the
  //                         order of descendents as they are executed, i.e. i_pipeStarts is decremented.

  CHK(pipe(filedes)); // creates pipe and populates filedes[0] filedes[1] with ptrs to read and write end of first pipe
  // fflush output streams before forking so they aren't copied to child if buffered
  fflush(stderr);
  fflush(stdout);
  CHK(middleChild = fork());
  if (0 == middleChild)
  { // second child logic starts here
    for (i = 0; i < numPipes - 1; i++)
    {                                   // repeat for however many middle children there are (if one pipe (numPipes - 1 == 0) and this is not executed)
      CHK(pipe(filedes + 2 * (i + 1))); // filedes[2] and filedes[3] are read/write ends of second pipe created (first commands), filedes[4] and [5] for third pipe, etc.
      fflush(stderr);                   // flush before forking (only writing to stderr was possible)
      CHK(lastChild = fork());          // fork so child and grandchild can handle respective jobs
      if (0 != lastChild)
      {                                                                                     // if the parent of the most recent fork
        CHK(dup2(filedes[2 * i + 1], STDOUT_FILENO));                                       // write end of the "right pipe" (if you're thinking about the command text left to right) is dup2'd to stdout
        CHK(dup2(filedes[2 * i + 2], STDIN_FILENO));                                        // read end of "left pipe" is dup2'd to stdin
        closeFileDescriptors(filedes, numFDs);                                              // close FD's after dup2's, remove duplicate pointers
        CHK(execvp(newargv[pipeStarts[i_pipeStarts]], &newargv[pipeStarts[i_pipeStarts]])); // replace process image to execute command, see notes at variable definition above
        exit(0);                                                                            // terminate after succesful execution. CHKs would terminate on failure
      }
      i_pipeStarts--; // see notes at variable definition above
    }
  }

  /*** START last child logic ***/
  if ((0 == lastChild && numPipes > 1) || (0 == middleChild && numPipes == 1))
  { // again, if one pipe, middleChild is actually the "last child"
    if (lessFlag)
    {                                           // if there's an input redirect
      CHK(input_fd = open(lessFlag, O_RDONLY)); // get file descriptor
      CHK(dup2(input_fd, STDIN_FILENO));        // redirect input
      close(input_fd);                          // duplicate pointers in file descriptor table so close it
    }
    CHK(dup2(filedes[numFDs - 1], STDOUT_FILENO)); // make stdout point to write end of last pipe created
    closeFileDescriptors(filedes, numFDs);
    CHK(execvp(newargv[0], newargv)); // replace process image to execute the command
    _exit(0);                         // terminate grandchild after successful completion. CHKs would terminate on failure
  }
  /*** END last child logic ***/

  /*** START FIRST CHILD LOGIC ***/
  // only the first child reaches this point
  if (greaterFlag)
  { // if there's an output redirect
    // note on following flags: need to write, create file if not there, make sure file doesn't exist already
    // user has read and write permissions
    oFlags = outputOverwrite ? O_WRONLY | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_EXCL;
    CHK(output_fd = open(greaterFlag, oFlags, S_IRUSR | S_IWUSR)); // get file descriptor
    CHK(dup2(output_fd, STDOUT_FILENO));                           // replace stdout with output_fd
    close(output_fd);                                              // close duplicate file descriptor entry
  }
  CHK(dup2(filedes[0], STDIN_FILENO)); // replace stdin filedescriptor with read end of pipe
  closeFileDescriptors(filedes, numFDs);
  CHK(execvp(newargv[pipeStarts[numPipes - 1]], &newargv[pipeStarts[numPipes - 1]])); // first child does the last command in pipeStarts, see
  _exit(0);                                                                           // terminate child after successful completion. CHKs terminate on failure
  // note: the parent waiting for the child is handled in main
  /*** End first child logic/pipe helper ***/
}

// Input: signum, which will be SIGTERM based on our usage
// Output: void
// Purpose: work with system call signal to prevent killing actual shell on SIGTERM
// note: blank function suffices to prevent p2 process termination
void signalHandler(int signum)
{
}

// Input: string to be analyzed
// Output: bool result of whether it's a metacharacter
// Purpose: check if a string is a metacharacter
int isMetaChar(char *buffPtr)
{
  return (strcmp(buffPtr, "&") == 0 || strcmp(buffPtr, "<") == 0 || strcmp(buffPtr, ">") == 0 || strcmp(buffPtr, ">!") == 0 || strcmp(buffPtr, "|") == 0);
}

// Output: 2 if first word is EOF, 1 for terminated line (based on getword.c), 0 for empty line, -1 for obvious input error
// Purpose: populate newargv per instructions, set flags for metacharacters
// notes:  buffPtr is incremented by wordLength + 1 to account for length of word and the null character
//         metacharacters are not inserted into newargv, instead flags are set to indicate to main how to process
//         redirectErrorFlag is set without immediately returning -1 in order to process all successive input on the line,
//             this ensures the next call of parse begins on the next line of input
int parse()
{
  int wordLength = 0;       // tracks length of word returned from getword
  char *buffPtr = &buff[0]; // used to track position in buff
  openSingleQuote = 0;      // set to 0 at start of parse, works with getword to verify no open single quotes, input error
  newargvIndex = 0;         // tracks number of args in newargv. note: by args here I mean commands and arguments to the commands

  /*** major for loop in parse. calls getword repeatedly, populates newargv, sets flags ***/
  for (; newargvIndex < MAX_ITEMS;)
  {
    wordLength = getword(buffPtr);
    if (wordLength == -1)
    { // EOF was encountered if -1. terminate newargv to allow for proper reading by execvp
      newargv[newargvIndex] = NULL;
      // if EOF on first word return 2 to indicate to main that we need to break out of main for loop
      if (newargvIndex == 0 && !flagPresent())
        return 2;
      // check for open single quot error and incomplete line
      if (openSingleQuote | (newargvIndex == 0 && flagPresent()))
        return -1;
      // otherwise return 1 b/c eof serves as line terminator
      return 1;
    }
    else if (wordLength == 0)
    {                               // getword returned 0 so check for empty line, improper input, or finished command
      newargv[newargvIndex] = NULL; // terminate expression in newargv
      if (newargvIndex == 0)
      {
        if (!flagPresent()) // no metacharacters and no arguments means an empty line. return 0 to tell main to continue
          return 0;
      }
      if (openSingleQuote || (newargvIndex == 0 && flagPresent())) // opensingle quote or improper input is error
        return -1;
      // otherwise return 1 b/c line has been parsed
      return 1;
    }
    // check for metacharacters, set applicable flag if encountered
    // metacharacters are not inserted into newargv (and therfore newargvIndex is not incremented)
    // instructions don't care about them but main() needs to know so flags are set instead
    else if (strcmp(buffPtr, "<") == 0)
    {
      // more than one input redirect per line is not allowed, return error if there is
      if (lessFlag)
      { // if it's already been set, that's an error because more than one < not allowed.
        buffPtr += wordLength + 1;
        while ((wordLength = getword(buffPtr) != 0) && wordLength != -1)
        {
          buffPtr += wordLength + 1;
        }
        fprintf(stderr, "Error: more than one input redirect now allowed\n");
        return -1;
      }
      buffPtr += wordLength + 1;
      wordLength = getword(buffPtr); // get the name for the redirect
      if (wordLength == -1 || wordLength == 0)
      { // EOF (not first word) return 1
        fprintf(stderr, "Error: no argument provided after <\n");
        return -1;
      }
      if (isMetaChar(buffPtr))
      {
        // process rest of line
        while ((wordLength = getword(buffPtr) != 0) && wordLength != -1)
        {
          buffPtr += wordLength + 1;
        }
        fprintf(stderr, "Error: missing name for redirect\n");
        return -1;
      }
      lessFlag = buffPtr; // assign name for redirect and set flag
      buffPtr += wordLength + 1;
      continue;
    }
    else if (strcmp(buffPtr, "|") == 0)
    {
      numPipes++;
      newargv[newargvIndex++] = NULL;            // insert null to terminate that command when reading from newargv
      pipeStarts[i_pipeStarts++] = newargvIndex; // saves index in newargv of where the command starts following a pipe (newargvIndex was incremented to location after \0);
      buffPtr += wordLength + 1;
      wordLength = getword(buffPtr); // check the next character for improper input
      if (isMetaChar(buffPtr))
      {
        // process rest of line if invalid argument so we are at the right place for next parse call
        while ((wordLength = getword(buffPtr) != 0) && wordLength != -1)
          buffPtr += wordLength + 1;
        fprintf(stderr, "Error: invalid null command\n");
        return -1;
      }
      else if (wordLength == -1 || wordLength == 0)
      { // Invalid argument (not provided)
        fprintf(stderr, "Error: invalid null command\n");
        return -1;
      }
      // otherwise add the next word to newargv
      newargv[newargvIndex++] = buffPtr;
      buffPtr += wordLength + 1;
      continue;
    }
    else if (strcmp(buffPtr, "&") == 0)
    {
      newargv[newargvIndex] = NULL;
      ampersandFlag = 1; // indicate to main that there is a background process ready to get started
      return 1;          // & serves as terminator so main can start executing the process and then start next one
    }
    else if (strcmp(buffPtr, ">") == 0 || strcmp(buffPtr, ">!") == 0)
    {
      if (greaterFlag)
      { // if it's already been set, that's an error because more than one output redirect not allowed
        buffPtr += wordLength + 1;
        // process rest of line if invalid argument so we are at the right place for next parse call
        while ((wordLength = getword(buffPtr) != 0) && wordLength != -1)
          buffPtr += wordLength + 1;
        fprintf(stderr, "Error: more than one output redirect now allowed\n");
        return -1;
      }
      if (strcmp(buffPtr, ">!") == 0)
        outputOverwrite = 1;
      buffPtr += wordLength + 1; // advance to next position following >!\0 to get where
      wordLength = getword(buffPtr);
      if (wordLength == -1 | wordLength == 0)
      { // EOF (not first word) return 1
        fprintf(stderr, "Error: missing name for redirect\n");
        return -1;
      }
      if (isMetaChar(buffPtr))
      {
        // process rest of line if invalid argument so we are at the right place for next parse call
        while ((wordLength = getword(buffPtr) != 0) && wordLength != -1)
          buffPtr += wordLength + 1;
        fprintf(stderr, "Error: missing name for redirect\n");
        return -1;
      }
      // set flag to the name for the redirect
      greaterFlag = buffPtr;
      buffPtr += wordLength + 1;
      continue;
    }
    // all special cases for returning or continuing handled above
    // insert into newargv, increment buffPtr and newargvIndex accordingly
    newargv[newargvIndex++] = buffPtr;
    buffPtr += wordLength + 1;
  }
  /*** end major for loop in parse ***/
  // this portion of code only accessed when maxargs is exceeded. special handling not required per professor
  fprintf(stderr, "Error: maximum arguments exceeded\n");
  return -1;
}

// input: argc, argv
// output: 0 on successful completion, or non-zero on error
// purpose: call parse and process input as a simple command line interpreter per instructions
int main(int argc, char *argv[])
{
  int parseVal = '\0'; // return value for parse method
  pid_t pid;           // temp pid used for checking return value of wait
  char *path;          // used in cd to get path to home and for /dev/null
  char absPath[MAX_PATH];
  DIR *dirp;           // DIR * used to process ls-F
  struct dirent *dp;   // used to cycle through contents of directory for ls-F
  struct stat statVal; // used with lstat to gain information on file/directory
  int output_fd;       // output file descriptor for redirecting output
  int input_fd;        // input file descriptor for redirecting input
  int i;               // use for loop tracking

  // set pgid of p2 to its own process group to prevent killing (outer) shell. see setpgid man for details
  // when p2 is terminated. exit if error
  CHK(setpgid(0, 0));
  // establish signal handling for SIGTERM signal. SIGTERM is used to kill process group
  if (signal(SIGTERM, signalHandler) == SIG_ERR)
  {
    fprintf(stderr, "Error setting up signal handler\n");
    exit(1);
  }

  /*** major for loop for sending input to parse and then processing it ***/
  for (;;)
  {
    // reset flags after each call to parse. should ampersand be set to 0 here??
    ampersandFlag = outputOverwrite = i_pipeStarts = numPipes = 0;
    greaterFlag = lessFlag = NULL;
    // issue prompt
    printf(":480: ");
    parseVal = parse();
    if (parseVal == -1)
    { // input error detected in parse perror, print error message and continue
      // input output redirect error messages handled in parse when detected.
      // these messages printed here prevent duplicate code parse in case of getword returning -1 or 0
      if (openSingleQuote)
        fprintf(stderr, "Error: unmatched single quote\n");
      if (newargvIndex == 0 && flagPresent()) // nothing inserted but metacharacters present not allowed
        fprintf(stderr, "Error: syntax issue, missing argument\n");
      continue;
    }
    if (parseVal == 0) // empty line, continue to next line
      continue;
    if (parseVal == 2) // EOF encountered on first word, exit loop
      break;
    if (openSingleQuote)
    { // openSingleQuote flag from getword is an error, continue
      fprintf(stderr, "Error: open single quote\n");
      continue;
    }

    // check newargv isn't null before looking into comparisons
    // this should've been handled in parse but here for safety
    if (!newargv[0])
    {
      fprintf(stderr, "Error populating newargv\n");
      continue;
    }

    /*** START cd LOGIC ***/

    if (strcmp(newargv[0], "cd") == 0)
    {
      // more than one directory argument is not allowed
      if (newargvIndex > 2)
      {
        fprintf(stderr, "Error: too many arguments to cd\n");
        continue;
      }
      if (newargvIndex == 1)
      { // no directory argument provided, therefore go get path to HOME and go there
        path = getenv("HOME");
        if (path == NULL)
        { // check if getenv failed to find home and print error
          fprintf(stderr, "Error: failed to get home path\n");
          continue;
        }
        if (chdir(path) < 0)
        { // chdir to home, report error if failed
          fprintf(stderr, "Error: failed to chdir to home\n");
          continue;
        }
        // cd was successful, continue to next iteration of for loop
        continue;
      }
      // if a directory argument was provided, we change to that
      if (newargvIndex == 2)
      {
        if (chdir(newargv[1]) < 0)
        {
          fprintf(stderr, "Error: failed to chdir to provided argument\n");
          continue;
        }
        // cd was sucessful, continue to next iteration of main for loop
        continue;
      }
    }

    /*** END cd LOGIC ***/

    /*** START ls-F LOGIC ***/

    if (strcmp(newargv[0], "ls-F") == 0)
    {
      if (newargvIndex == 1)
      { // only ls-F is in newargv, no file/directory, therefore list contents of current directory
        if ((dirp = opendir(".")) == NULL)
        { // if dirp is null, there was an error. print and continue
          fprintf(stderr, "Error: failed to open current directory\n");
          continue;
        }
        // loop through directory, printing file names.
        while (dirp)
        {
          if ((dp = readdir(dirp)) != NULL)
          {
            printf("%s", dp->d_name);
            path = dp->d_name;
            if (lstat(path, &statVal) < 0)
              printf("&\n");
            else if (S_ISDIR(statVal.st_mode))
              printf("/\n");
            else if (S_IFLNK == (statVal.st_mode & S_IFMT))
            {
              if (stat(path, &statVal) < 0)
                printf("&\n");
              printf("@\n");
            }
            // 73 == bitmask to find if executable bits are set. if non-zero, one of the x bits are set and the statement is true
            else if (statVal.st_mode & 73)
              printf("*\n");
            else
              printf("\n");
          }
          else
          { // reached end of directory b/c dp is null. break while loop and continue
            closedir(dirp);
            break;
          }
        }
        continue; // end of ls-F with for current directory, go to next iteration of main for loop
      }
      // >= 1 directory/file argument provided to ls-F is
      // only have to check first argument and print the contents of that directory like above
      // first check if it exists and get info on it using lstat
      if (newargvIndex > 1)
      {
        for (i = 1; i < newargvIndex; i++)
        {
          if (lstat(newargv[i], &statVal) < 0)
          { // doesn't exist, print error and name, and continue
            fprintf(stderr, "Error: could not find %s\n", newargv[1]);
            continue;
          }
          if (S_ISDIR(statVal.st_mode))
          { // check if it's a directory. (S_ISDIR returns non-zero if it is)
            if ((dirp = opendir(newargv[i])) == NULL)
            { // // open to print and read contents. if null, there was an error
              fprintf(stderr, "Error: failed to open directory %s\n", newargv[1]);
              continue;
            }
            // loop through directory, printing file names and appending symbols per instructions.
            while (dirp)
            {
              if ((dp = readdir(dirp)) != NULL)
              {
                printf("%s", dp->d_name);                 // print the name of entry
                path = realpath(newargv[i], absPath);     // get the absolute path
                sprintf(path, "%s/%s", path, dp->d_name); // append filename onto absolute path to it to use with lstat
                if (lstat(path, &statVal) < 0)
                  printf("&\n");
                else if (S_ISDIR(statVal.st_mode))
                  printf("/\n");
                else if (S_IFLNK == (statVal.st_mode & S_IFMT))
                {
                  if (stat(path, &statVal) < 0)
                    printf("&\n");
                  else
                    printf("@\n");
                }
                // 73 == bitmask to find if executable bits are set. if non-zero, one of the x bits are set and the statement is true
                else if (statVal.st_mode & 73) // *checked last for proper printing behavior
                  printf("*\n");
                else
                  printf("\n");
              }
              else
              { // reached end of directory b/c dp is null. break while loop and continue
                closedir(dirp);
                break;
              }
            }
            continue; // end of ls-F with for current directory, go to next iteration of main for loop
          }
          // if it does exist, is not a directory, and can be accessed with lstat, print it per instructions
          printf("%s\n", newargv[i]);
        }
        continue;
      }
    }

    /*** END ls-F LOGIC ***/

    /*** START exec LOGIC ***/

    if (strcmp(newargv[0], "exec") == 0)
    {
      if (execvp(newargv[1], &newargv[1]) < 0)
      { // replace process image and provide arguments
        fprintf(stderr, "Error: unable to execute commmand\n");
        continue;
      }
    }

    /*** END exec LOGIC ***/

    // all other actions require forking

    // fflush output streams before forking so they aren't copied to child if buffered.
    fflush(stderr);
    fflush(stdout);
    CHK(childPid = (int)fork());

    /*** START CHILD LOGIC ***/

    if (childPid == 0)
    {

      // go to helper function to handle piping
      // note: forking happens before piping because of vertical piping
      //       children and grandchildren terminate in the helper function
      //           so the logic that follows these lines only applies if no pipe
      if (numPipes)
        pipeHelper();

      // if no other input redirect and it's a background process,
      // redirect input to /dev/null to prevent so background jobs cannot read from terminal
      // inspired from: https://stackoverflow.com/questions/229012/getting-absolute-path-of-a-file
      if (!lessFlag && ampersandFlag)
      {
        if ((path = realpath("/dev/null", absPath)) == NULL)
        {
          fprintf(stderr, "Error getting path to /dev/null\n");
          _exit(1);
        }
        CHK(input_fd = open(path, O_RDONLY));
        CHK(dup2(input_fd, STDIN_FILENO));
        close(input_fd);
      }
      // redirect input / output if necessary, if no pipe is set
      if (lessFlag)
      {
        CHK(input_fd = open(lessFlag, O_RDONLY)); // get file descriptor. only need to read
        CHK(dup2(input_fd, STDIN_FILENO));        // redirect input to stdin
      }
      // redirect output as requested
      if (greaterFlag)
      {
        // note on following flags: need to write, create file if not there, make sure file doesn't exist already
        // user has read and write permissions
        oFlags = outputOverwrite ? O_WRONLY | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_EXCL;
        if ((output_fd = open(greaterFlag, oFlags, S_IRUSR | S_IWUSR)) < 0)
        {
          if (errno == EISDIR)
            fprintf(stderr, "Error: %s is a directory\n", greaterFlag);
          else if (errno == EEXIST)
            fprintf(stderr, "Error file exists\n");
          else
            fprintf(stderr, "Error opening %s\n", greaterFlag);
          exit(1);
        }
        CHK(dup2(output_fd, STDOUT_FILENO)); // replace stdout with output fd
      }
      // both are opened first and then closed if necessary b/c
      // if they opened and closed then opened and closed they would have same fd
      if (lessFlag)
        close(input_fd); // duplicate pointers in file descriptor table, close it
      if (greaterFlag)
        close(output_fd);               // duplicate pointers in file descriptor table, close it
      CHK(execvp(newargv[0], newargv)); // replace process image to execute command
      _exit(0);                         // terminate child on success. would have terminated on failure with CHKs
    }

    /*** END CHILD LOGIC ***/

    /*** START PARENT LOGIC ***/

    else
    { // else shouldn't be necessary. only parent should reach here
      if (!ampersandFlag)
      { // if the flag for background process isn't set, wait for child to finish
        for (;;)
        {
          CHK(pid = wait(NULL)); // call wait continuously. NULL argument used because we don't need extra status info. reaps zombies
          if (pid == childPid)
            break; // break from for loop when childPid is returned, indicating its terminated
        }
        continue; // ready for next input now, continue to next iteration of main for loop
      }
      // normal case handled above in !ampersandFlag, background process logic here:
      // parent prints newargv[0] of child followed by PID of child in brackets
      // and then onto the next iteration of the main for loop
      printf("%s [%d]\n", newargv[0], childPid);
    }
    /*** END PARENT LOGIC ***/
  }
  /***  END MAIN FOR LOOP (above curly brace) ***/

  // terminate any children that are still running. p2 process catches sigterm and continues to run.
  killpg(getpgrp(), SIGTERM);
  printf("p2 terminated.\n");
  exit(0); // success!
}
