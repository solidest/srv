#include "src/sds.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <fstream>
#include <string>
#include <iostream>

#include <uv.h>


using namespace std;

static int tests = 0, fails = 0;
#define test(_s) { printf("#%02d ", ++tests); printf(_s); }
#define test_cond(_c) if(_c) printf("\033[0;32mPASSED\033[0;0m\n"); else {printf("\033[0;31mFAILED\033[0;0m\n"); fails++;}



int main(int argcs, char **argvs) {
    cout << "ok" << endl;
}
