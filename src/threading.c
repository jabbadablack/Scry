#include <engine/threading.h>
#include <flecs.h>
#include <stdio.h>
#include <stdlib.h>

void ScryThreading_Init(int num_threads) {
    (void)num_threads;
    // TODO:Task pool init logic
}

void ScryThreading_Shutdown(void) {
    // TODO:Task pool shutdown logic
}

void ScryThreading_SetFlecsOSAPI(void) {
    // TODO:Patch flecs os api logic
}
