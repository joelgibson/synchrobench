#include <stdio.h>

#include "random.h"

unsigned long r = 54321;

unsigned long next(void) {
	return r = ((r*1103515245)+12345) & 0x7fffffff;
}

double unif(void) {
	return (double)next() / (double)0x7fffffff;
}

#define nelem(a) (sizeof(a)/(sizeof(a[0])))
int bins[20];

int main(void) {
	for (int i = 0; i < 10000; i++) {
		double u = unif();
		bins[(int)(u*nelem(bins))]++;
	}
	double step = 1.0 / nelem(bins);
	for (int i = 0; i < nelem(bins); i++) {
		printf("%.2f - %.2f: %d\n", i*step, (i+1)*step, bins[i]);
	}
	return 0;
}