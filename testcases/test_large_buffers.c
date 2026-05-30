// Test 5: Large stack buffers in a single function
// Expected: process_data = 2048+1024+1024 = 4096 bytes
// Threshold 1024B -> OVERFLOW
void process_data() {
    char input_buf[2048];
    char output_buf[1024];
    int  temp[256];
    input_buf[0] = 0;
    output_buf[0] = 0;
    temp[0] = 0;
}
int main() {
    process_data();
    return 0;
}
