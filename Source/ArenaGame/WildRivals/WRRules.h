#pragma once
// Engine-independent rules shared by the actual GameMode/creature and small C++ tests.
namespace WRRules
{
    inline int WinnerAtTimeout(int P1Captures, int P2Captures)
    { return P1Captures == P2Captures ? -1 : (P1Captures > P2Captures ? 0 : 1); }

    inline int WildSpawnBudget(int Requested, int AliveWild, int Capacity)
    {
        const int Space = Capacity > AliveWild ? Capacity - AliveWild : 0;
        if (Requested <= 0) return 0;
        return Requested < Space ? Requested : Space;
    }

    inline bool CanRecruitCompanion(bool IsWildAggressive, int WinnerTeam, bool HasLiveCompanion)
    { return IsWildAggressive && WinnerTeam >= 0 && WinnerTeam < 2 && !HasLiveCompanion; }
}
