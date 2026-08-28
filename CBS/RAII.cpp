#include <iostream>
#include <memory>
#include <mutex>
#include <string>

using namespace std;
#ifndef NODE_H
#define NODE_H

class Node
{
  private:


  public:
    Node() { 
      cout << "unnamed Node is allocated." << endl;
    };
    Node(string str) { 
      name = str;
      cout << "Named Node is allocated." << endl;
    };
    explicit (Node(int));
    void intro() {
      printf("Hello everyone, my name is %s\n", name.c_str());
    }

    virtual ~Node(){
      cout << name << "Node is deallocated." << endl;
    }
    string name = "anomaly";
};

#endif /* NODE_H */
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
    return 0;
  }

  delete steve;

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
