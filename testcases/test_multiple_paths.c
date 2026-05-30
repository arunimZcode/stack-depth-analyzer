// Test 4: Multiple call paths - analyzer must find worst case
// light path: dispatcher(64) + light_func(32)  = 100B
// heavy path: dispatcher(64) + heavy_func(512) = 580B
// Expected worst: main(4) + dispatcher(64) + heavy_func(512) = 580B
void light_func() {
    char buf[32];
    buf[0] = 1;
}
void heavy_func() {
    char buf[512];
    buf[0] = 2;
}
void dispatcher(int mode) {
    char buf[64];
    if (mode == 0) light_func();
    else heavy_func();
}
int main() {
    dispatcher(0);
    dispatcher(1);
    return 0;
}
