#include <stdio.h>

int cal(int a, int b)
{
  int c = a + b;
  int d = c * c;
  return d;
}
int main(int argc, char *argv[])
{
  cal(3,4);
  return 0;
}

/*
 [observation]
 o0
 - operation 'add' has been call two times,but only one add expression in my code
 - every single operation result will be stored for next usage
 - 
 [hypothesise]



 */
