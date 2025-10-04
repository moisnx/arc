#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// This function calculates the factorial of a given integer n
long long factorial(int n) {
    long long result = 1;

    // A factorial of a negative number doesn't exist in this context,
    // and 0! is 1. We handle the positive case with a loop.
    if (n < 0) {
        return -1; // Or some other way to indicate an error
    } else if (n == 0) {
        return 1;
    } else {
        for (int i = 1; i <= n; ++i) {
            result *= i;
        }
        return result;
    }
}

int main() {
    int num;
    long long fact;

    printf("Enter a non-negative integer to find its factorial: ");

    // Get user input
    if (scanf("%d", &num) != 1) {
        printf("Invalid input. Please enter a valid integer.\n");
        return 1; // Indicate an error
    }

    // Call the factorial function
    fact = factorial(num);

    if (fact == -1) {
        printf("Factorial of a negative number is not defined.\n");
    } else {
        printf("The factorial of %d is %lld.\n", num, fact);
    }

    return 0; // Indicate successful execution
}
