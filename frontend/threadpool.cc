#include "threadpool.h"

Threadpool::Threadpool(int num){
  threadNum = num;
}

int Threadpool::getThreadNum(){
  return threadNum;
}
