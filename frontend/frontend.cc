#include <stdio.h>
#include "threadpool.h"
#include "debug.h"

int main(){
  Threadpool pool(10);
  debug(1, "Thread number is: %d\n", pool.getThreadNum());
  return 0;
}
