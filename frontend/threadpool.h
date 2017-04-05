#ifndef THREADPOOL_H
#define THREADPOOL_H

class Threadpool {
  int thread_num;

public:
  Threadpool(int num);
  int get_thread_num();

};

#endif
