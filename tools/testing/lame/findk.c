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
    int num = (int)n * x_percent * a * b;  // numerator before /100
    int rhs = num / 100 - (int)n * a;
    int den = b - a;

    int k_est = rhs / den;  // floor division
    if (k_est < 0) k_est = 0;
    if (k_est > n) k_est = n;

    // Round to nearest integer by checking k_est and k_est+1
    int best_k = k_est;
    int best_diff = 1e9;  

    for (int cand = k_est; cand <= k_est + 1 && cand <= n; cand++) {
        int sum_num = cand * b + (n - cand) * a; // numerator
        int sum_den = a * b;                     // denominator

        // compare |sum/den/n - x/100|
        // cross-multiply diff = |sum_num*100 - n*x_percent*sum_den|
        int diff = abs(sum_num * 100 - (int)n * x_percent * sum_den);

        if (diff < best_diff) {
            best_diff = diff;
            best_k = cand;
        }
    }

    float avg = ((float)best_k/a + (float)(n-best_k)/b) / (float)n;
    printf("[(%d, %d), %d][%f][%f]\n", a, b, best_k, avg*100, best_diff);
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <n>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i <= 100; i++) {
        findk(i, atoi(argv[1]));
    }
    return 0;
}