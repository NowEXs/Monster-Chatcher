#include "../Source/ArenaGame/WildRivals/WRRules.h"
#include <cassert>
#include <iostream>
int main()
{
    // Time limit results, including 0-0 and ties at the goal.
    assert(WRRules::WinnerAtTimeout(0, 0) == -1);
    assert(WRRules::WinnerAtTimeout(4, 2) == 0);
    assert(WRRules::WinnerAtTimeout(2, 4) == 1);
    assert(WRRules::WinnerAtTimeout(5, 5) == -1);
    // Taming frees a wild slot, but a wave never exceeds its configured allocation.
    assert(WRRules::WildSpawnBudget(2, 2, 2) == 0);
    assert(WRRules::WildSpawnBudget(2, 1, 2) == 1);
    assert(WRRules::WildSpawnBudget(2, 0, 2) == 2);
    assert(WRRules::WildSpawnBudget(2, 3, 2) == 0);
    assert(WRRules::WildSpawnBudget(-1, 0, 2) == 0);
    for (int Cap = 0; Cap <= 12; ++Cap)
        for (int Alive = 0; Alive <= 16; ++Alive)
            for (int Request = 0; Request <= 12; ++Request)
            {
                int N = WRRules::WildSpawnBudget(Request, Alive, Cap);
                assert(N >= 0 && N <= Request);
                if (Alive < Cap) assert(Alive + N <= Cap);
                else assert(N == 0);
            }
    assert(WRRules::CanRecruitCompanion(true, 0, false));
    assert(WRRules::CanRecruitCompanion(true, 1, false));
    assert(!WRRules::CanRecruitCompanion(true, 0, true));
    assert(!WRRules::CanRecruitCompanion(false, 0, false));
    assert(!WRRules::CanRecruitCompanion(true, 2, false));
    assert(!WRRules::CanRecruitCompanion(true, 255, false));
    std::cout << "PASS: timeout results, wave-cap invariants (2873 cases), companion ownership limits\n";
}
