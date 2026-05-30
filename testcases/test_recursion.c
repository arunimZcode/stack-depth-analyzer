// Test 3: Recursion detection
// Expected: [RECURSION] warning at factorial, analysis completes safely
int factorial(int n) {
    char buf[64];
    (void)buf;
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
int fibonacci(int n) {
    char buf[32];
    (void)buf;
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
}
int main() {
    return factorial(5) + fibonacci(8);
}
