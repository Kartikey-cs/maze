#include "maze_model.h"
#include <stdio.h>
int main(void)
{
    double features[MAZE_MODEL_FEATURE_COUNT];
    for (;;)
    {
        int first = scanf("%lf", &features[0]);
        if (first == EOF) return ferror(stdin) ? 1 : 0;
        if (first != 1) return 1;
        for (int i = 1; i < MAZE_MODEL_FEATURE_COUNT; i++)
            if (scanf("%lf", &features[i]) != 1) return 1;
        printf("%.17g\n", MazeModel_PredictSuccess(features));
    }
}
