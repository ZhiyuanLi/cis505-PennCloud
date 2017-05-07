#include "conhash.h"

int hash_str(const char *s) {
  unsigned h = FIRSTH;
  while (*s) {
    h = (h * A) ^ (s[0] * B);
    s++;
  }
  return (int)(h % MAX_KEY);
}
