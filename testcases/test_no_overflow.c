// Test 6: Baseline - no overflow
// Expected: all chains well within 8192B threshold
// Overflow risks = 0
void helper() {
    int x = 1, y = 2, z = 3;
    (void)(x+y+z);
}
void worker() {
    int arr[16];
    arr[0] = 0;
    helper();
}
void task_a() {
    int val = 0;
    worker();
    (void)val;
}
void task_b() {
    char small[8];
    small[0] = 0;
}
int main() {
    task_a();
    task_b();
    return 0;
}
