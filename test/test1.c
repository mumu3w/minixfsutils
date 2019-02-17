#include <stdio.h>
#include <string.h>


#define DIRSIZ 14

static inline int min(int x, int y)
{
	return x < y ? x : y;
}

// check if s is an empty string
int is_empty(char *s)
{
  return *s == 0;
}

// check if c is a path separator
int is_sep(char c)
{
  return c == '/';
}

// adapted from skipelem in xv6/fs.c
char *skipelem(char *path, char *name)
{
  while (is_sep(*path))
    path++;
  char *s = path;
  while (!is_empty(path) && !is_sep(*path))
    path++;
  int len = min(path - s, DIRSIZ);
  memmove(name, s, len);
  if (len < DIRSIZ)
    name[len] = 0;
  return path;
}

/*
	string = "/root/test/bin"		skipelem return = "/test/bin"		name = "root"
	string = "root/test/bin"		skipelem return = "/test/bin"		name = "root"
	string = "///root//test//bin"	skipelem return = "//test//bin"		name = "root"
	string = ""						skipelem return = "" 				name = ""
	string = "///root//test//bin/"  skipelem return = "//test//bin/"	name = "root"
	string = "bin/"  				skipelem return = "/" 				name = "bin"
	string = "/"  					skipelem return = "" 				name = ""
*/
int main(void)
{
	char name[DIRSIZ+1];
	char *s;
	char *s1 = "/root/test/bin";
	char *s2 = "root/test/bin";
	char *s3 = "///root//test//bin";
	char *s4 = "";
	char *s5 = "///root//test//bin/";
	char *s6 = "bin/";
	char *s7 = "/";
	
	name[DIRSIZ] = 0;
	
	s = skipelem(s1, name);
	printf(" string = \"%s\" ", s1);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s2, name);
	printf(" string = \"%s\" ", s2);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s3, name);
	printf(" string = \"%s\" ", s3);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s4, name);
	printf(" string = \"%s\" ", s4);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s5, name);
	printf(" string = \"%s\" ", s5);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s6, name);
	printf(" string = \"%s\" ", s6);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	s = skipelem(s7, name);
	printf(" string = \"%s\" ", s7);
	printf(" skipelem return = \"%s\" name = \"%s\"\n", s, name);
	
	return 0;
}
