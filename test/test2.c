#include <stdio.h>


#undef min
static inline int min(int x, int y)
{
  return x < y ? x : y;
}

#undef max
static inline int max(int x, int y)
{
  return x > y ? x : y;
}

// ceiling(x / y) where x >=0, y >= 0
static inline int divceil(int x, int y)
{
    return x == 0 ? 0 : (x - 1) / y + 1;
}

/*
  0  0
  1  1
  1  1
  2  2
  2  2
  2  2
  3  3
*/
int main(void)
{
  int n ;
  int nd;
  
  n = divceil(0, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(1023, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(1024, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(1025, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(2047, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(2048, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  n = divceil(2049, 1024);
  nd = min(n, 7);
  printf("%d  %d\n", n, nd);
  
  return 0;
}