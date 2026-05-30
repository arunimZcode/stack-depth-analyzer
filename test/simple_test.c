// Test: a->b->c call chain
void c_func() {
    char buf[512];
    buf[0] = 0;
}

void b_func() {
    char buf[256];
    c_func();
}

void a_func() {
    char buf[128];
    b_func();
}

int main() {
    a_func();
    return 0;
}
