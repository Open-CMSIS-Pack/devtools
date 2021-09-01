#include "calc.h"

int Add(int x, int y) {
  return x+y; 
}

int Sub(int x, int y) {
  return (x>y) ? (x-y) : (y-x);
}

int Mul(int x, int y) {
  return x*y; 
}

int Div(int x, int y) {
  if (x == 0 || y == 0) {
    return 0;
  }
  return x/y; 
}
