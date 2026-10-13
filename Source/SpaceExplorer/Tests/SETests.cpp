// Automation tests: run in the editor via Tools > Session Frontend > Automation, filter "SpaceExplorer".
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "SEGalaxySubsystem.h"
#include "SETerrainChunk.h"
#include "SEInventoryComponent.h"
#include "SEVitalsComponent.h"
#include "SEProgression.h"
#include "SECreature.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEGalaxyDeterminismTest, "SpaceExplorer.Galaxy.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSEGalaxyDeterminismTest::RunTest(const FString& Parameters)
{
    USEGalaxySubsystem* A = NewObject<USEGalaxySubsystem>();
    USEGalaxySubsystem* B = NewObject<USEGalaxySubsystem>();
    for (int32 Idx : { 0, 5, 777, 1999 })
    {
        const FSEStarSystemData X = A->BuildSystem(Idx), Y = B->BuildSystem(Idx);
        TestEqual(TEXT("system name"), X.Name, Y.Name);
        TestEqual(TEXT("planet count"), X.Planets.Num(), Y.Planets.Num());
        TestTrue(TEXT("planet count in range"), X.Planets.Num() >= 2);
        for (int32 i = 0; i < X.Planets.Num(); ++i) { TestEqual(TEXT("planet seed"), X.Planets[i].Seed, Y.Planets[i].Seed); }
    }
    const FSEPlanetData Home = A->BuildSystem(0).Planets[0];
    TestTrue(TEXT("home planet is temperate"), Home.Biome == ESEBiome::Temperate);
    TestTrue(TEXT("home planet is breathable"), Home.bBreathable);
    B->GalaxySeed = 42;
    TestTrue(TEXT("different seed, different galaxy"), !A->GetSystemPosition(10).Equals(B->GetSystemPosition(10)));
    TestEqual(TEXT("nearest list size"), A->GetNearestSystems(0, 8).Num(), 8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETerrainTest, "SpaceExplorer.World.TerrainSeams", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSETerrainTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("height deterministic"), SETerrain::Height(1234.f, -999.f, 77, ESEBiome::Volcanic), SETerrain::Height(1234.f, -999.f, 77, ESEBiome::Volcanic));
    const int32 Res = 17; const float Size = 6400.f;
    FSETerrainMeshData Left, Right;
    ASETerrainChunk::BuildMeshData(FIntPoint(0, 0), Size, Res, 99, ESEBiome::Temperate, Left);
    ASETerrainChunk::BuildMeshData(FIntPoint(1, 0), Size, Res, 99, ESEBiome::Temperate, Right);
    TestEqual(TEXT("vertex count includes skirts"), Left.Vertices.Num(), Res * Res + 4 * Res);
    TestTrue(TEXT("has triangles"), Left.Triangles.Num() > 0 && Left.Triangles.Num() % 3 == 0);
    for (int32 iy = 0; iy < Res; ++iy)
    {
        const float ZLeft = Left.Vertices[iy * Res + (Res - 1)].Z, ZRight = Right.Vertices[iy * Res].Z;
        TestTrue(TEXT("neighbouring chunks share edge heights"), FMath::IsNearlyEqual(ZLeft, ZRight, 0.01f));
    }
    for (int32 i = 0; i < Left.Normals.Num(); i += 7) { TestTrue(TEXT("unit normals"), FMath::IsNearlyEqual(Left.Normals[i].Size(), 1.f, 0.01f)); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEInventoryTest, "SpaceExplorer.Gameplay.Inventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSEInventoryTest::RunTest(const FString& Parameters)
{
    USEInventoryComponent* Inv = NewObject<USEInventoryComponent>();
    Inv->MaxSlots = 2; Inv->MaxStackSize = 10;
    TestEqual(TEXT("add"), Inv->AddItem("iron_ore", 4), 4);
    TestEqual(TEXT("stack cap"), Inv->AddItem("iron_ore", 20), 6);
    TestEqual(TEXT("second slot"), Inv->AddItem("carbon", 1), 1);
    TestEqual(TEXT("slot cap"), Inv->AddItem("ice", 1), 0);
    TestFalse(TEXT("cannot remove more than held"), Inv->RemoveItem("carbon", 2));
    TestTrue(TEXT("remove"), Inv->RemoveItem("carbon", 1));
    TestEqual(TEXT("slot freed"), Inv->GetCount("carbon"), 0);
    TestEqual(TEXT("bad input ignored"), Inv->AddItem(NAME_None, 5), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEVitalsTest, "SpaceExplorer.Gameplay.Vitals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSEVitalsTest::RunTest(const FString& Parameters)
{
    USEVitalsComponent* V = NewObject<USEVitalsComponent>();
    V->SetVitals(50.f, 100.f, 10.f);
    TestFalse(TEXT("not enough energy"), V->ConsumeEnergy(20.f));
    TestTrue(TEXT("enough energy"), V->ConsumeEnergy(5.f));
    V->Heal(1000.f);
    TestEqual(TEXT("heal clamps"), V->Health, V->MaxHealth);
    V->ApplyDamage(1000.f);
    TestTrue(TEXT("dies"), V->IsDead());
    V->Heal(10.f);
    TestTrue(TEXT("dead players are not healed"), V->IsDead());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEMissionTest, "SpaceExplorer.Gameplay.Missions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSEMissionTest::RunTest(const FString& Parameters)
{
    USEProgression* P = NewObject<USEProgression>();
    const TArray<FName> Species = { FName("sp_1_0") };
    const TArray<FSEMission> Offers = USEProgression::GenerateMissions(555, FName("wayfarers"), TEXT("Tester"), 0, 0, FVector::ZeroVector, Species);
    TestEqual(TEXT("three offers"), Offers.Num(), 3);
    TestEqual(TEXT("deterministic"), USEProgression::GenerateMissions(555, FName("wayfarers"), TEXT("Tester"), 0, 0, FVector::ZeroVector, Species)[0].Title, Offers[0].Title);

    FSEMission M; M.Id = FName("t1"); M.Type = 0; M.Target = FName("iron_ore"); M.Required = 3; M.RewardCredits = 50; M.RewardRep = 7; M.Faction = FName("verdant");
    TestTrue(TEXT("accept"), P->AcceptMission(M));
    TestFalse(TEXT("no duplicates"), P->AcceptMission(M));
    P->ProgressMissions(0, FName("carbon"), 5);
    TestEqual(TEXT("wrong item ignored"), P->Data.ActiveMissions[0].Progress, 0);
    const int32 Credits = P->Data.Credits;
    P->ProgressMissions(0, FName("iron_ore"), 3);
    TestEqual(TEXT("mission completed"), P->Data.ActiveMissions.Num(), 0);
    TestEqual(TEXT("credits rewarded"), P->Data.Credits, Credits + 50);
    TestEqual(TEXT("reputation rewarded"), P->GetRep(FName("verdant")), 7);
    TestTrue(TEXT("cannot retake completed"), P->IsMissionTaken(M.Id));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSECreatureGenTest, "SpaceExplorer.Creatures.Species", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSECreatureGenTest::RunTest(const FString& Parameters)
{
    USEGalaxySubsystem* G = NewObject<USEGalaxySubsystem>();
    const FSEPlanetData Home = G->BuildSystem(0).Planets[0];
    const TArray<FSESpecies> A = SECreatureGen::Generate(Home), B = SECreatureGen::Generate(Home);
    TestEqual(TEXT("four archetypes on a living world"), A.Num(), 4);
    for (int32 i = 0; i < A.Num(); ++i) { TestEqual(TEXT("deterministic names"), A[i].Name, B[i].Name); }
    FSEPlanetData Rock = Home; Rock.Biome = ESEBiome::Barren; Rock.bHasAtmosphere = false;
    TestEqual(TEXT("harsh worlds have fewer species"), SECreatureGen::Generate(Rock).Num(), 2);
    return true;
}
#endif
