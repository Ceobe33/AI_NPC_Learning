#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <memory>
#include <string>

using namespace std;

class Node {
private:
  

  string name = "nobody";
public:
  Node() = delete;
  Node(string n_) {
    name = n_;
    printf("I'm %s here.\n", name.c_str());
  }
  void Greeting() {
    printf("%s is plan a vocation.\n", name.c_str());
  }
    virtual ~Node() {
      printf("%s here, see ya.\n", name.c_str());
    }
};

#endif /* MAIN_H */
int main(int argc, char *argv[])
{
  shared_ptr<Node> steve = make_shared<Node>("Steve");
  steve->Greeting();

  return 0;
}
/*

## cmake build
normal build
-rw-------. 1   5390 Aug 28 08:14 Makefile
-rw-------. 1   2255 Aug 28 08:14 cmake_install.cmake
-rwx------. 1 133360 Aug 28 08:33 hc

debug build

-rw-------. 1 u0_a248 u0_a248   5390 Aug 28 08:59 Makefile
-rw-------. 1 u0_a248 u0_a248   2246 Aug 28 08:59 cmake_install.cmake
-rwx------. 1 u0_a248 u0_a248 141776 Aug 28 08:59 hc

release build

-rw-------. 1 u0_a248 u0_a248  5390 Aug 28 09:01 Makefile
-rw-------. 1 u0_a248 u0_a248  2248 Aug 28 09:01 cmake_install.cmake
-rwx------. 1 u0_a248 u0_a248 51656 Aug 28 09:01 hc

## clang++ build

normal

-rwx------. 1 u0_a248 u0_a248 67424 Aug 28 09:04 a.out

-E

-rw-------. 1 u0_a248 u0_a248 2.2M Aug 28 09:10 clangE.log

-S
-rw-------. 1 u0_a248 u0_a248    0 Aug 28 09:10 clangS.log
-rw-------. 1 u0_a248 u0_a248 132K Aug 28 09:10 main.s

-c

-rw-------. 1 u0_a248 u0_a248   44136 Aug 28 09:24 main.o
-rw-------. 1 u0_a248 u0_a248  134890 Aug 28 09:10 main.s
*/
