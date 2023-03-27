// Devin Cline
// Professor Carroll
// CS480
// Due: 9/12/21

#include "getword.h"

extern int openSingleQuote;
/*
*  Input: char * from the driver that is used to store word for printing
*  Output: length of word or special cases per instructions of assignment
*  Purpose: analyze input for analysis and printing per instructions of assignment
*  notes to explain things below without repetitive comments:
*          '\0' is used to terminate words in C
*          !openSingleQuote in if statement means not in a single quote, so we're not allowing special characters inserted
*          (backslash allowing for special characters is handled in its own if statements)
*          39 is the int ascii for single quote. it is expressed that way to avoid a warning caused from using "'"
*          when non-null characters are added to the array, length is incremented to track word length
*/
int getword(char* w) {
  int iochar = '\0';  // char to scan in and evaluate
  int length = 0; // tracks the length of word

   // boolean to track whether inside a single quote word to allow for special rules

  // loop through input until EOF
  while ((iochar = getchar()) != EOF) {

    // if the buffer is full, terminate the word and place the character back on the stream
    // to be read as start of next new word
    if (length == STORAGE - 1) {
      w[length] = '\0';
      ungetc(iochar, stdin);
      return length;
    }

    // skip leading spaces before a word (thus the length == 0)
    // if booleans are true, spaces can be be added in those special cases
    if (iochar == ' ' && length == 0 && !openSingleQuote) {
      continue;
    }

    // if "'" is encountered, toggle openSingleQuote to allow/disallow special character insertion
    // this allows for words in single quotes to be added in front/within/at the end of other words
    // which is the desired behavior for the shell
    if (iochar == 39) {
      openSingleQuote = !openSingleQuote;
      continue;
    }

    // if within single quote and there is backslash, check the next character
    // if it's a "'" add that instead of the backslash per instructions
    // otherwise add the backslash and put the character back on the stream to be processed next
    if (iochar == '\\' && openSingleQuote) {
      iochar = getchar();
      if (iochar == 39) {
        w[length++] = 39;
        continue;
      }
      w[length++] = '\\';
      ungetc(iochar, stdin);
      continue;
    }

    // if iochar is backslash, check next character
    // break if it's eof because nothing needs to be added or length changed
    // if it's \n or ; we terminate and replace back on the stream b/c it serves as delimiter per instructions
    // otherwise put in any character, increment length, and continue to next character
    if (iochar == '\\' && !openSingleQuote) {
      iochar = getchar();
      if (iochar == EOF) {
        break;
      }
      if (iochar == '\n' || iochar == ';') {
        w[length] = '\0';
        if (length > 0) {
          ungetc(iochar, stdin);
        }
        return length;
      }
      w[length++] = iochar;
      continue;
    }

    // not in single quote, iochar is  ' ' and serving as delimiter
    // terminate string and return length
    if (length > 0 && iochar == ' ' && !openSingleQuote) {
      w[length] = '\0';
      return length;
    }

    // \n and ; always terminates string
    // if length > 0 place it back on the stream to be processed per instructions
    if (iochar == '\n' || iochar == ';') {
      w[length] = '\0';
      if (length > 0)
        ungetc(iochar, stdin);
      return length;
    }

    // check if iochar is '>' check length to make sure it's already been read as a delimiter
    // check next character if > is encounterd to see if it's a ! because greedy design looks ahead
    // if it is !, insert both, incrementing length twice and return length
    // otherwise we just insert > and put the next character back on the stream to look at individually next iteration
    if (length == 0 && iochar == '>' && !openSingleQuote) {
      iochar = getchar();
      if (iochar == '!') {
        w[length++] = '>';
        w[length++] = '!';
        w[length] = '\0';
        return length;
      }
      ungetc(iochar, stdin);
      w[length++] = '>';
      w[length] = '\0';
      return length;
    }

    // normal case: length > 0 so iochar is a delimiter / metacharacter that needs to be pushed back onto stream and read individually
    // terminate word, put iochar back onto stream to be read next iteration and return length
    if (length > 0 && (iochar == '<' || iochar == '|' || iochar == '&' || iochar == '>') && !openSingleQuote) {
      w[length] = '\0';
      ungetc(iochar, stdin);
      return length;
    }

    // normal case: word size is 0 and single metacharacter is encountered ('>' handled separately), put the character in the buffer
    // and return length, which will be one
    if (length == 0 && (iochar == '<' || iochar == '|' || iochar == '&') && !openSingleQuote) {
      w[length++] = iochar;
      w[length] = '\0';
      return length;
    }

    // all logic has been accounted for
    // add iochar to the array
    w[length++] = iochar;
  }

  // EOF has has been encountered because the loop was exited, therefore terminate word
  w[length] = '\0';

  // if word length is 0 after EOF is encountered, return -1, which is desired behavior
  if (length == 0) {
    return -1;
  }

  // otherwise return word length
  return length;
}
