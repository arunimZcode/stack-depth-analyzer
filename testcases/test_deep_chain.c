// Test 2: Deep 5-level chain
// Expected: main(4B)->a(64B)->b(128B)->c(256B)->d(512B) = 964 bytes
// Threshold 512B -> OVERFLOW
void d_func() {
    char buf[512];
    buf[0] = 0;
}
void c_func() {
    char buf[256];
    d_func();
}
void b_func() {
    char buf[128];
    c_func();
}
void a_func() {
    char buf[64];
    b_func();
}
int main() {
    int x = 0;
    a_func();
    return x;
}
