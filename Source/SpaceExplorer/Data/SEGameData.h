#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SEGameData.generated.h"

struct FSEItemDef      { FName Id; FString Name; FString Desc; int32 BasePrice = 10; bool bUsable = false; };
struct FSERecipe       { FName Id; FString Name; TMap<FName, int32> Inputs; FName Output; int32 OutputCount = 1; bool bNeedsFabricator = false; FName RequiredTech; };
struct FSETechNode     { FName Id; FString Name; FString Desc; int32 Cost = 10; TArray<FName> Prereqs; };
struct FSEStructureDef { FName Id; FString Name; FVector Size = FVector(2, 2, 2); TMap<FName, int32> Cost; FLinearColor Color = FLinearColor::Gray; FName RequiredTech; FString Desc; };
struct FSEFaction      { FName Id; FString Name; FString Blurb; FLinearColor Color; };

/** All static game content (items, recipes, tech, structures, factions, prices), defined in code so the project needs no data assets. */
UCLASS()
class SPACEEXPLORER_API USEGameData : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    TMap<FName, FSEItemDef> Items;
    TArray<FSERecipe> Recipes;
    TArray<FSETechNode> Tech;
    TArray<FSEStructureDef> Structures;
    TArray<FSEFaction> Factions;

    FString ItemName(FName Id) const;
    const FSETechNode* FindTech(FName Id) const;
    const FSEStructureDef* FindStructure(FName Id) const;
    const FSEFaction* FindFaction(FName Id) const;

    /** Planet-specific price modifier (0.8-1.3) * markup, adjusted by reputation. */
    int32 BuyPrice(FName Item, int32 PlanetSeed, int32 Rep) const;
    int32 SellPrice(FName Item, int32 PlanetSeed, int32 Rep) const;
private:
    float PlanetModifier(FName Item, int32 PlanetSeed) const;
};
