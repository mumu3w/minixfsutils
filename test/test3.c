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

char *splitpath(char *path, char *dirbuf, size_t size)
{
  char *s = path, *t = path;
  while(!is_empty(path)) {
    while(is_sep(*path))
      path++;
     s = path;
    while(!is_empty(path) && !is_sep(*path))
      path++;
  }
  if(dirbuf != NULL) {
    int n = min(s - t, size - 1);
    memmove(dirbuf, t, n);
    dirbuf[n] = 0;
  }
  return s;
}

/*
 str = "/root/test/bin"       splitpath return = "bin"  name = "/root/test/"
 str = "root/test/bin"        splitpath return = "bin"  name = "root/test/"
 str = "///root//test//bin"   splitpath return = "bin"  name = "///root//test//"
 str = ""                     splitpath return = ""     name = ""
 str = "///root//test//bin/"  splitpath return = ""     name = "///root//test//bin/"
 str = "bin/"                 splitpath return = ""     name = "bin/"
 str = "/"                    splitpath return = ""     name = "/"
*/
int main(void)
{
	char name[30];
	char *s;
	char *s1 = "/root/test/bin";
	char *s2 = "root/test/bin";
	char *s3 = "///root//test//bin";
	char *s4 = "";
	char *s5 = "///root//test//bin/";
	char *s6 = "bin/";
	char *s7 = "/";
	
	name[29] = 0;
	
	s = splitpath(s1, name, 29);
	printf(" string = \"%s\" ", s1);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s2, name, 29);
	printf(" string = \"%s\" ", s2);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s3, name, 29);
	printf(" string = \"%s\" ", s3);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s4, name, 29);
	printf(" string = \"%s\" ", s4);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s5, name, 29);
	printf(" string = \"%s\" ", s5);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s6, name, 29);
	printf(" string = \"%s\" ", s6);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	s = splitpath(s7, name, 29);
	printf(" string = \"%s\" ", s7);
	printf(" splitpath return = \"%s\" name = \"%s\"\n", s, name);
	
	return 0;
}
