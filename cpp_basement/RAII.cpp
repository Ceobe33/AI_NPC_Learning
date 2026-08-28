#include <iostream>
#include <memory>
#include <mutex>
#include <string>

using namespace std;
#ifndef __RAII_H__
#define __RAII_H__
#define TAG "RAII"

class Node
{
  private:


  public:
    Node() { 
      cout << "anomaly Node is allocated." << endl;
      name = "anomaly";
    };
    Node(string str) { 
      cout << "Named Node is allocated." << endl;
      name = str;
    };
    void intro() {
      printf("Hello everyone, my name is %s\n", name.c_str());
    }

    virtual ~Node(){
      cout << name << " Node is deallocated." << endl;
    }
    string name;
    Node* next_node;
};

#endif /* ifndef __RAII_H__ */

int main(int argc, char *argv[]){
  // smart pointers
  int a = 3;
  unique_ptr<int> uni_a = make_unique<int>();

  printf("number a is: %d, unique ptr a is: %d\n", a, *uni_a);

  // mutexes manager locks

  


  Node* steve = new Node("Steve");
  unique_ptr<Node> ashely = make_unique<Node>("Ashely");
  steve->intro();
  ashely->intro();

  if (true) {
    printf("%s module didn't end properly.\n", TAG);
    return 0;
  }

  delete steve;

  printf("%s module end properly.\n", TAG);
  return 0;
}


  // what is the RAII?
  /* RAII is Resources aqcauisition in Initialization
  * is a type of tech for resources management
  * also called Scope-bounced lifecycle Management
  * the key feature is 
  * 1. allocate in constructor
  *    if abort when obj constructing is doesn't matter
  *    cause the deallocator will clear the memory
  * 2. deallocate in deconstructor
  //                           *
  * */
