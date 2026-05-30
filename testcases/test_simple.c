// Test 1: Simple 3-level call chain
// Expected: main(4B) -> a_func(128B) -> b_func(256B) = 388 bytes
// Threshold 512B -> no overflow
void b_func() {
    char buf[256];
    buf[0] = 0;
}
void a_func() {
    char buf[128];
    b_func();
}
int main() {
    int x = 0;
    a_func();
    return x;
}
