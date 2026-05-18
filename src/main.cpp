#include <string.h>
#include <stdio.h>

extern int demo_simple_subpasses(int argc, char** argv);
extern int demo_compute_postprocess(int argc, char** argv);
extern int demo_headless_compute(int argc, char** argv);

int run_single_demo(int (*fptr)(int, char**), const char* name, const char* testname,
                    int mode, int argc, char** argv) {
    if (mode == 0) {
        if (strcmp(name, testname) != 0) return -1;
        printf("Launching demo '%s'...\n\n", testname);
        int ret = fptr(argc, argv);
        printf("Demo '%s' finished with exit code %d.\n", testname, ret);
        return 0;
    } else if (mode == 1) {
        printf("  %s\n", testname);
        return -1;
    }
    return 0;
}

#define DO_DEMO(demo_name) if (run_single_demo(demo_name, name, #demo_name, mode, argc, argv) >= 0) return 0;

int run_demos(const char* name, int mode, int argc, char** argv) {
    DO_DEMO(demo_simple_subpasses);
    DO_DEMO(demo_compute_postprocess);
    DO_DEMO(demo_headless_compute);
    return -1;
}

int main(int argc, char** argv) {
    printf("\n===== Lepton2 demo launcher =====\n");
    if (argc < 2) {
        printf("Not enough input arguments. The following demos are available:\n");
        run_demos("", 1, argc, argv);
    } else {
        int ret = run_demos(argv[1], 0, argc, argv);
        if (ret < 0) {
            printf("Unrecognized demo '%s'. The following demos are available:\n", argv[1]);
            run_demos("", 1, argc, argv);
        }
    }
    return 0;
}