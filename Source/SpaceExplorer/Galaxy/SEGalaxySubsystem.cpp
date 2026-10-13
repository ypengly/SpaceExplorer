#include "SEGalaxySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "SpaceExplorer.h"

namespace
{
    struct FStarClass { const TCHAR* Name; float Luminosity; FLinearColor Color; };
    const FStarClass StarClasses[] = {
        { TEXT("M red dwarf"),    0.05f, FLinearColor(1.0f, 0.45f, 0.25f) },
        { TEXT("K orange dwarf"), 0.4f,  FLinearColor(1.0f, 0.7f, 0.4f) },
        { TEXT("G yellow star"),  1.0f,  FLinearColor(1.0f, 0.95f, 0.75f) },
        { TEXT("F white star"),   2.5f,  FLinearColor(0.95f, 0.95f, 1.0f) },
        { TEXT("A blue-white"),   10.f,  FLinearColor(0.7f, 0.8f, 1.0f) },
    };

    FString MakeName(FRandomStream& R)
    {
        static const TCHAR* Syl[] = { TEXT("ka"),TEXT("ve"),TEXT("lor"),TEXT("an"),TEXT("thi"),TEXT("mus"),TEXT("or"),TEXT("zen"),
                                      TEXT("dra"),TEXT("syl"),TEXT("quo"),TEXT("rin"),TEXT("tal"),TEXT("eph"),TEXT("bu"),TEXT("nex") };
        const int32 N = R.RandRange(2, 3);
        FString Out;
        for (int32 i = 0; i < N; ++i) { Out += Syl[R.RandRange(0, UE_ARRAY_COUNT(Syl) - 1)]; }
        Out[0] = FChar::ToUpper(Out[0]);
        return Out;
    }

    ESEBiome PickBiome(FRandomStream& R, float TempC)
    {
        struct FW { ESEBiome B; float W; };
        TArray<FW> T;
        if (TempC > 150.f)      T = { {ESEBiome::Volcanic, .45f}, {ESEBiome::Barren, .35f}, {ESEBiome::Desert, .2f} };
        else if (TempC > 45.f)  T = { {ESEBiome::Desert, .5f}, {ESEBiome::Volcanic, .2f}, {ESEBiome::Barren, .3f} };
        else if (TempC > -10.f) T = { {ESEBiome::Temperate, .35f}, {ESEBiome::Ocean, .3f}, {ESEBiome::Desert, .2f}, {ESEBiome::Barren, .15f} };
        else                    T = { {ESEBiome::Frozen, .6f}, {ESEBiome::Barren, .3f}, {ESEBiome::Ocean, .1f} };
        float Roll = R.FRand(), Acc = 0.f;
        for (const FW& E : T) { Acc += E.W; if (Roll <= Acc) return E.B; }
        return T.Last().B;
    }
}

FVector USEGalaxySubsystem::GetSystemPosition(int32 Index) const
{
    FRandomStream R(GetTypeHash(FIntPoint(GalaxySeed, Index * 7 + 1)));
    const int32 Arm = Index % 4;
    const float Rad01 = FMath::Sqrt(R.FRand());
    const float Angle = Arm * HALF_PI + Rad01 * 3.2f + R.FRandRange(-0.35f, 0.35f);
    const float Dist = Rad01 * GalaxyRadiusLY;
    const float Z = R.FRandRange(-1.f, 1.f) * 600.f * (1.f - Rad01);
    if (Index == 0) return FVector::ZeroVector;
    return FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, Z);
}

FSEStarSystemData USEGalaxySubsystem::BuildSystem(int32 Index) const
{
    Index = FMath::Clamp(Index, 0, SystemCount - 1);
    FSEStarSystemData Sys;
    Sys.Index = Index;
    Sys.Seed = GetTypeHash(FIntPoint(GalaxySeed, Index));
    Sys.PositionLY = GetSystemPosition(Index);
    FRandomStream R(Sys.Seed);

    Sys.Name = MakeName(R);
    const int32 ClassIdx = (Index == 0) ? 2 : R.RandRange(0, UE_ARRAY_COUNT(StarClasses) - 1);
    const FStarClass& SC = StarClasses[ClassIdx];
    Sys.StarClass = SC.Name;
    Sys.StarColor = SC.Color;
    Sys.AsteroidFieldCount = R.RandRange(0, 3);

    const int32 PlanetCount = R.RandRange(2, 8);
    for (int32 i = 0; i < PlanetCount; ++i)
    {
        FSEPlanetData P;
        P.Seed = GetTypeHash(FIntPoint(Sys.Seed, i + 100));
        FRandomStream PR(P.Seed);
        P.Name = Sys.Name + TEXT(" ") + FString::FromInt(i + 1);
        P.OrbitAU = 0.35f * FMath::Sqrt(SC.Luminosity) * FMath::Pow(1.55f, (float)i) * PR.FRandRange(0.85f, 1.15f);
        const float TempK = 278.f * FMath::Pow(SC.Luminosity, 0.25f) / FMath::Sqrt(P.OrbitAU);
        P.TemperatureC = TempK - 273.15f;
        P.Biome = PickBiome(PR, P.TemperatureC);
        P.RadiusKm = PR.FRandRange(2500.f, 9000.f);
        P.Gravity = FMath::Clamp(P.RadiusKm / 6371.f * PR.FRandRange(0.8f, 1.2f), 0.3f, 2.2f);
        P.bHasAtmosphere = (P.Biome != ESEBiome::Barren);
        P.bBreathable = (P.Biome == ESEBiome::Temperate || P.Biome == ESEBiome::Ocean) && PR.FRand() < 0.7f;
        Sys.Planets.Add(P);
    }

    // Moons for some planets.
    const int32 BaseCount = Sys.Planets.Num();
    for (int32 i = 0; i < BaseCount; ++i)
    {
        if (R.FRand() > 0.35f) continue;
        const int32 MoonCount = R.RandRange(1, 2);
        for (int32 m = 0; m < MoonCount; ++m)
        {
            FSEPlanetData Moon;
            Moon.Seed = GetTypeHash(FIntPoint(Sys.Seed, 1000 + i * 4 + m));
            FRandomStream MR(Moon.Seed);
            Moon.Name = Sys.Planets[i].Name + (m == 0 ? TEXT(" I") : TEXT(" II"));
            Moon.bIsMoon = true;
            Moon.OrbitAU = Sys.Planets[i].OrbitAU;
            Moon.TemperatureC = Sys.Planets[i].TemperatureC;
            Moon.Biome = (Moon.TemperatureC < -10.f && MR.FRand() < 0.6f) ? ESEBiome::Frozen : ESEBiome::Barren;
            Moon.RadiusKm = MR.FRandRange(600.f, 2200.f);
            Moon.Gravity = MR.FRandRange(0.12f, 0.35f);
            Moon.bHasAtmosphere = false;
            Moon.bBreathable = false;
            Sys.Planets.Add(Moon);
        }
    }

    // The home system always starts the player on a breathable, temperate world.
    if (Index == 0)
    {
        FSEPlanetData& Home = Sys.Planets[0];
        Home.Name = TEXT("Haven");
        Home.Biome = ESEBiome::Temperate;
        Home.OrbitAU = 1.f; Home.TemperatureC = 15.f; Home.Gravity = 1.f; Home.RadiusKm = 6371.f;
        Home.bHasAtmosphere = true; Home.bBreathable = true;
        Sys.Name = TEXT("Solenne");
    }
    return Sys;
}

TArray<int32> USEGalaxySubsystem::GetNearestSystems(int32 FromIndex, int32 Count) const
{
    const FVector Origin = GetSystemPosition(FromIndex);
    TArray<TPair<float, int32>> Dist;
    Dist.Reserve(SystemCount);
    for (int32 i = 0; i < SystemCount; ++i)
    {
        if (i == FromIndex) continue;
        Dist.Emplace(FVector::DistSquared(Origin, GetSystemPosition(i)), i);
    }
    Dist.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
    TArray<int32> Out;
    for (int32 i = 0; i < FMath::Min(Count, Dist.Num()); ++i) Out.Add(Dist[i].Value);
    return Out;
}

FSEPlanetData USEGalaxySubsystem::GetCurrentPlanet() const
{
    const FSEStarSystemData Sys = BuildSystem(CurrentSystem);
    return Sys.Planets[FMath::Clamp(CurrentPlanet, 0, Sys.Planets.Num() - 1)];
}

void USEGalaxySubsystem::SetCurrent(int32 System, int32 Planet)
{
    CurrentSystem = FMath::Clamp(System, 0, SystemCount - 1);
    const int32 Num = BuildSystem(CurrentSystem).Planets.Num();
    CurrentPlanet = FMath::Clamp(Planet, 0, Num - 1);
}

bool USEGalaxySubsystem::TravelTo(int32 System, int32 Planet)
{
    if (System < 0 || System >= SystemCount) return false;
    if (Planet < 0 || Planet >= BuildSystem(System).Planets.Num()) return false;
    SetCurrent(System, Planet);
    const FString Level = UGameplayStatics::GetCurrentLevelName(this, true);
    UE_LOG(LogSpaceExplorer, Log, TEXT("Traveling to system %d planet %d (reloading %s)"), System, Planet, *Level);
    UGameplayStatics::OpenLevel(this, FName(*Level));
    return true;
}
