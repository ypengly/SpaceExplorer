#include "SEGameData.h"
#include <utility>
#include <initializer_list>

namespace
{
    TMap<FName, int32> Map(std::initializer_list<std::pair<const TCHAR*, int32>> L)
    {
        TMap<FName, int32> Out;
        for (const auto& P : L) Out.Add(FName(P.first), P.second);
        return Out;
    }
}

void USEGameData::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    auto Item = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, int32 Price, bool bUse = false)
    {
        FSEItemDef D; D.Id = FName(Id); D.Name = Name; D.Desc = Desc; D.BasePrice = Price; D.bUsable = bUse;
        Items.Add(D.Id, D);
    };
    Item(TEXT("iron_ore"), TEXT("Iron Ore"), TEXT("Common metal ore. Smelt into plates."), 6);
    Item(TEXT("copper_ore"), TEXT("Copper Ore"), TEXT("Conductive ore for circuits."), 8);
    Item(TEXT("carbon"), TEXT("Carbon"), TEXT("Fuel feedstock and building material."), 5);
    Item(TEXT("ice"), TEXT("Ice"), TEXT("Frozen water. Source of breathable oxygen."), 4);
    Item(TEXT("silicon"), TEXT("Silicon"), TEXT("Electronics grade crystal."), 7);
    Item(TEXT("uranium"), TEXT("Uranium"), TEXT("Dense, valuable and mildly radioactive."), 40);
    Item(TEXT("biomass"), TEXT("Biomass"), TEXT("Organic matter. Edible in an emergency (+20 health)."), 9, true);
    Item(TEXT("alien_artifact"), TEXT("Alien Artifact"), TEXT("Technology of an unknown civilization."), 120);
    Item(TEXT("iron_plate"), TEXT("Iron Plate"), TEXT("Structural metal plate."), 15);
    Item(TEXT("circuit"), TEXT("Circuit"), TEXT("Electronic component for advanced builds."), 45);
    Item(TEXT("fuel_cell"), TEXT("Fuel Cell"), TEXT("Refuels the ship (press R in the cockpit)."), 30);
    Item(TEXT("oxygen_canister"), TEXT("Oxygen Canister"), TEXT("Restores 60 oxygen."), 25, true);
    Item(TEXT("repair_kit"), TEXT("Repair Kit"), TEXT("Restores 50 health, or 50 ship hull while piloting."), 40, true);

    auto Recipe = [this](const TCHAR* Id, const TCHAR* Name, TMap<FName, int32> In, const TCHAR* Out, int32 N, bool bFab, const TCHAR* Tech = TEXT(""))
    {
        FSERecipe R; R.Id = FName(Id); R.Name = Name; R.Inputs = In; R.Output = FName(Out); R.OutputCount = N; R.bNeedsFabricator = bFab;
        R.RequiredTech = FName(Tech);
        Recipes.Add(R);
    };
    Recipe(TEXT("iron_plate"), TEXT("Iron Plate"), Map({ {TEXT("iron_ore"), 2} }), TEXT("iron_plate"), 1, false);
    Recipe(TEXT("fuel_cell"), TEXT("Fuel Cell"), Map({ {TEXT("carbon"), 3}, {TEXT("iron_plate"), 1} }), TEXT("fuel_cell"), 1, false);
    Recipe(TEXT("oxygen_canister"), TEXT("Oxygen Canister"), Map({ {TEXT("ice"), 2}, {TEXT("iron_plate"), 1} }), TEXT("oxygen_canister"), 1, false);
    Recipe(TEXT("circuit"), TEXT("Circuit"), Map({ {TEXT("copper_ore"), 2}, {TEXT("silicon"), 1} }), TEXT("circuit"), 1, true);
    Recipe(TEXT("repair_kit"), TEXT("Repair Kit"), Map({ {TEXT("iron_plate"), 2}, {TEXT("circuit"), 1} }), TEXT("repair_kit"), 1, true);

    auto Tech_ = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, int32 Cost, std::initializer_list<const TCHAR*> Pre)
    {
        FSETechNode T; T.Id = FName(Id); T.Name = Name; T.Desc = Desc; T.Cost = Cost;
        for (const TCHAR* P : Pre) T.Prereqs.Add(FName(P));
        Tech.Add(T);
    };
    Tech_(TEXT("mining_2"), TEXT("Mining Laser II"), TEXT("Multitool mines and damages 60% more."), 15, {});
    Tech_(TEXT("thermal_protection"), TEXT("Thermal Protection"), TEXT("Survive extreme hot and cold planets."), 20, {});
    Tech_(TEXT("oxygen_tank"), TEXT("Oxygen Tank II"), TEXT("+50 maximum oxygen."), 20, {});
    Tech_(TEXT("base_building_2"), TEXT("Advanced Habitats"), TEXT("Unlocks the Oxygen Beacon and Fabricator."), 25, {});
    Tech_(TEXT("engine_1"), TEXT("Ship Engine I"), TEXT("+12% ship speed."), 25, {});
    Tech_(TEXT("engine_2"), TEXT("Ship Engine II"), TEXT("+12% ship speed."), 45, { TEXT("engine_1") });
    Tech_(TEXT("fuel_1"), TEXT("Fuel Tank I"), TEXT("+25% fuel capacity."), 25, {});
    Tech_(TEXT("fuel_2"), TEXT("Fuel Tank II"), TEXT("+25% fuel capacity."), 45, { TEXT("fuel_1") });
    Tech_(TEXT("shield_1"), TEXT("Shield I"), TEXT("+30% shield capacity."), 30, {});
    Tech_(TEXT("shield_2"), TEXT("Shield II"), TEXT("+30% shield capacity."), 50, { TEXT("shield_1") });
    Tech_(TEXT("cargo_1"), TEXT("Cargo Bay I"), TEXT("+5 cargo slots."), 25, {});
    Tech_(TEXT("cargo_2"), TEXT("Cargo Bay II"), TEXT("+5 cargo slots."), 45, { TEXT("cargo_1") });

    auto Struct = [this](const TCHAR* Id, const TCHAR* Name, FVector Size, TMap<FName, int32> Cost, FLinearColor Col, const TCHAR* Tech, const TCHAR* Desc)
    {
        FSEStructureDef S; S.Id = FName(Id); S.Name = Name; S.Size = Size; S.Cost = Cost; S.Color = Col; S.RequiredTech = FName(Tech); S.Desc = Desc;
        Structures.Add(S);
    };
    Struct(TEXT("foundation"), TEXT("Foundation"), FVector(4, 4, 0.4f), Map({ {TEXT("iron_plate"), 2} }), FLinearColor(0.5f, 0.5f, 0.55f), TEXT(""), TEXT("Flat base platform."));
    Struct(TEXT("wall"), TEXT("Wall"), FVector(4, 0.4f, 3), Map({ {TEXT("iron_plate"), 1} }), FLinearColor(0.6f, 0.6f, 0.65f), TEXT(""), TEXT("Wall segment."));
    Struct(TEXT("habitat"), TEXT("Habitat"), FVector(5, 5, 3), Map({ {TEXT("iron_plate"), 6}, {TEXT("silicon"), 4} }), FLinearColor(0.3f, 0.6f, 0.9f), TEXT(""), TEXT("Pressurised shelter: breathable air and slow healing nearby."));
    Struct(TEXT("solar"), TEXT("Solar Generator"), FVector(2, 2, 0.5f), Map({ {TEXT("iron_plate"), 3}, {TEXT("silicon"), 4} }), FLinearColor(0.1f, 0.15f, 0.5f), TEXT(""), TEXT("Recharges your energy nearby."));
    Struct(TEXT("oxygen_beacon"), TEXT("Oxygen Beacon"), FVector(1, 1, 3), Map({ {TEXT("iron_plate"), 4}, {TEXT("circuit"), 2} }), FLinearColor(0.2f, 0.9f, 0.8f), TEXT("base_building_2"), TEXT("Makes the air breathable within 30 m."));
    Struct(TEXT("fabricator"), TEXT("Fabricator"), FVector(3, 2, 2), Map({ {TEXT("iron_plate"), 5}, {TEXT("circuit"), 2}, {TEXT("copper_ore"), 4} }), FLinearColor(0.9f, 0.6f, 0.2f), TEXT("base_building_2"), TEXT("Unlocks advanced crafting within 15 m."));

    auto Faction = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Blurb, FLinearColor C)
    {
        FSEFaction F; F.Id = FName(Id); F.Name = Name; F.Blurb = Blurb; F.Color = C; Factions.Add(F);
    };
    Faction(TEXT("wayfarers"), TEXT("Wayfarers Guild"), TEXT("Traders and navigators who keep the lanes open."), FLinearColor(0.9f, 0.75f, 0.2f));
    Faction(TEXT("verdant"), TEXT("Verdant Collective"), TEXT("Scientists and biologists studying every living thing."), FLinearColor(0.3f, 0.8f, 0.4f));
    Faction(TEXT("syndicate"), TEXT("Iron Syndicate"), TEXT("Miners and engineers who take what the ground gives."), FLinearColor(0.8f, 0.35f, 0.3f));
}

FString USEGameData::ItemName(FName Id) const
{
    const FSEItemDef* D = Items.Find(Id);
    return D ? D->Name : Id.ToString();
}
const FSETechNode* USEGameData::FindTech(FName Id) const { for (const FSETechNode& T : Tech) if (T.Id == Id) return &T; return nullptr; }
const FSEStructureDef* USEGameData::FindStructure(FName Id) const { for (const FSEStructureDef& S : Structures) if (S.Id == Id) return &S; return nullptr; }
const FSEFaction* USEGameData::FindFaction(FName Id) const { for (const FSEFaction& F : Factions) if (F.Id == Id) return &F; return nullptr; }

float USEGameData::PlanetModifier(FName Item, int32 PlanetSeed) const
{
    FRandomStream R(GetTypeHash(FIntPoint(PlanetSeed, GetTypeHash(Item))));
    return R.FRandRange(0.8f, 1.3f);
}
int32 USEGameData::BuyPrice(FName Item, int32 PlanetSeed, int32 Rep) const
{
    const FSEItemDef* D = Items.Find(Item);
    if (!D) return 0;
    const float RepMod = 1.f - FMath::Clamp(Rep, -100, 100) * 0.001f; // friendly = up to 10% cheaper
    return FMath::Max(1, FMath::RoundToInt(D->BasePrice * PlanetModifier(Item, PlanetSeed) * 1.15f * RepMod));
}
int32 USEGameData::SellPrice(FName Item, int32 PlanetSeed, int32 Rep) const
{
    const FSEItemDef* D = Items.Find(Item);
    if (!D) return 0;
    const float RepMod = 1.f + FMath::Clamp(Rep, -100, 100) * 0.001f;
    return FMath::Max(1, FMath::RoundToInt(D->BasePrice * PlanetModifier(Item, PlanetSeed) * 0.85f * RepMod));
}
