#include <engine/threading.h>
#include <flecs.h>
#include <stdio.h>
#include <stdlib.h>

void ScryThreading_Init(int num_threads) {
    (void)num_threads;
    // Task pool init logic here
}

void ScryThreading_Shutdown(void) {
    // Task pool shutdown logic here
}

void ScryThreading_SetFlecsOSAPI(void) {
    // Patch flecs os api logic here
}
