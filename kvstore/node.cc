#include <string.h>
#include <map>

#include "node.h"

Node::Node(string key) {
    prev = NULL;
    next = NULL;
    this->key = key; // user 
    map<string, string> value; //key:filename, value: value
    this->value = value;
}

// for head and tail
Node::Node() {
    prev = NULL;
    next = NULL;
}