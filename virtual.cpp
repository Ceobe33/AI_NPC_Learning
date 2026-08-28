#include <stdio.h>
#include <iostream>

class Base
{
private:
  

public:
  Base(){
    printf("base class constructed\n");
  };
  virtual ~Base(){
    printf("base class deconstructed\n");
  };
};


class Derived: public Base
{
private:
  

public:
  Derived(){
    printf("derived class constructed\n");
  };
  virtual ~Derived(){
    printf("derived class deconstructed\n");
  };
};

int main(int , char *[])
{
  Base* b = new Derived();

  delete b; 

  return 0;
}
