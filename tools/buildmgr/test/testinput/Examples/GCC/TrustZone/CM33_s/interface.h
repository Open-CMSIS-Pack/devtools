// function pointer
typedef int(*funcptr)(int,int);

/* Non-secure callable functions */
extern int function1(int x);
extern int function2(funcptr callback, int x, int y);

