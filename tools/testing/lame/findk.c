#include <stdio.h>
#include <stdlib.h>

void findk_old(int x_percent, int n) {
    
    int r = 100 / x_percent;
    int a = r; 
    int b = r + 1;

    int k = (((n*x_percent)/100*a*b)-(n*a)) / (b-a);
    printf("(%d, %d), %d\n", a, b, k);

    float avg = ((float)k/a + (float)(n-k)/b) / n;
    printf("avg: %f\n", avg);
}

void findk(int x_percent, int n) {
    // target fraction = x_percent / 100
    int r = 100 / x_percent;   // floor approx
    int a = r;
    int b = r + 1;
    if (a < 1) a = 1;

    // Compute ideal k (using integer math, careful with scaling)
    long num = (long)n * x_percent * a * b;  // numerator before /100
    long rhs = num / 100 - (long)n * a;
    long den = b - a;

    long k_est = rhs / den;  // floor division
    if (k_est < 0) k_est = 0;
    if (k_est > n) k_est = n;

    // Round to nearest integer by checking k_est and k_est+1
    long best_k = k_est;
    long best_diff = 1e18;

    for (long cand = k_est; cand <= k_est + 1 && cand <= n; cand++) {
        long sum_num = cand * b + (n - cand) * a; // numerator
        long sum_den = a * b;                     // denominator

        // compare |sum/den/n - x/100|
        // cross-multiply diff = |sum_num*100 - n*x_percent*sum_den|
        long diff = labs(sum_num * 100 - (long)n * x_percent * sum_den);

        if (diff < best_diff) {
            best_diff = diff;
            best_k = cand;
        }
    }

    printf("(%d, %d), %d\n", a, b, best_k);

    float avg = ((float)best_k/a + (float)(n-best_k)/b) / n;
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