#include "threadpool.h"

Threadpool::Threadpool(int num){
  thread_num = num;
}

int Threadpool::get_thread_num(){
  return thread_num;
}
