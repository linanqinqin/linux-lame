#include <stdio.h>
#include <stdlib.h>

void findk(int x_percent, int n) {
    
    int r = 100 / x_percent;
    int a = r; 
    int b = r + 1;

    int k = (((n*x_percent)/100*a*b)-(n*a)) / (b-a);
    printf("(%d, %d), %d\n", a, b, k);

    float avg = ((float)k/a + (float)(n-k)/b) / n;
    printf("avg: %f\n", avg);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <x_percent> <n>\n", argv[0]);
        return 1;
    }
    findk(atoi(argv[1]), atoi(argv[2]));
    return 0;
}