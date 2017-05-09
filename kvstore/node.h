#ifndef NODE_H
#define NODE_H

#include <string.h>
#include <map>

using namespace std;

class Node {

public:
    Node *prev;
    Node *next;
    string key;
    map<string, string> value;

public:
  	Node(string key);
  	Node();

};

#endif