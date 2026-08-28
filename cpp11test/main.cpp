#include <iostream>
#include <stdio.h>
#include <typeinfo>
#include <string>

using namespace std;

int main(int argc, char *argv[])
{
  int a = 10;
  const int& b = a;
  auto c = b;           // c 的类型是？
  decltype(b) d = a;    // d 的类型是？
  decltype((a)) e = a;  // e 的类型是？
               
  string str = "somthing like this";
  long l = 3;

  cout << typeid(c).name() << endl;
  cout << typeid(d).name() << endl;
  cout << typeid(e).name() << endl;
  cout << typeid(str).name() << endl;
  cout << typeid(l).name() << endl;
  cout << typeid(str).name() << endl;
  // const type_info& ti = typeid(a);
  // const type_info& ti1 = typeid(a);
  // cout << typeid(ti) << endl;
  // cout << typeid(ti1) << endl;
  // cout << type(e) << endl;
  // printf("param c type is %s\n,param d type is %s\n, param e type is %s\n", typeid(c).name(), typeid(d).name(),typeid(e).name());
  return 0;
}
