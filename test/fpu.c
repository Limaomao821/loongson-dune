#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // strcmp ..
#include <stdbool.h> // bool false true
#include <stdlib.h> // sort
#include <limits.h> // INT_MAX
#include <math.h> // sqrt
#include <unistd.h> // sleep
#include <assert.h> // assert
#include <fcntl.h> // open

int dune_enter();

void g(double a)
{
	a = a + 1.0;
}
int main(int argc, char *argv[])
{
	// 找到 gcc 内联汇编
	double x = 12.0;
	g(x);
	if (dune_enter()) {
		return 1;
	}
	x = x + 1;
	printf("x=%lf\n", x);
	return 0;
}
