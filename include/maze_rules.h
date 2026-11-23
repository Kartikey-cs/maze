#ifndef MAZE_RULES_H
#define MAZE_RULES_H
#define MAZE_STAMINA_MAX 100
#define MAZE_FRUIT_STAMINA 20
static inline int MazeRules_Move(int stamina)
{
    return stamina > 0 ? stamina - 1 : 0;
}
static inline int MazeRules_Collect(int stamina)
{
    int restored = stamina + MAZE_FRUIT_STAMINA;
    return restored < MAZE_STAMINA_MAX ? restored : MAZE_STAMINA_MAX;
}
#endif
