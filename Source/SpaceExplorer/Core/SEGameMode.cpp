#include "SEGameMode.h"
#include "SEPlayerCharacter.h"
#include "SEPlayerController.h"
#include "SEHUD.h"
#include "SEShip.h"
#include "SETerrainStreamer.h"
#include "SEPopulator.h"
#include "SEEnvironment.h"
#include "SEWorldObjects.h"
#include "SEGalaxySubsystem.h"
#include "SESaveSubsystem.h"
#include "SEProgression.h"
#include "SpaceExplorer.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

ASEGameMode::ASEGameMode()
{
    DefaultPawnClass = ASEPlayerCharacter::StaticClass();
    PlayerControllerClass = ASEPlayerController::StaticClass();
    HUDClass = ASEHUD::StaticClass();
}

void ASEGameMode::PreInitializeComponents()
{
    Super::PreInitializeComponents();
    USEGalaxySubsystem* Galaxy = GetGameInstance() ? GetGameInstance()->GetSubsystem<USEGalaxySubsystem>() : nullptr;
    if (!Galaxy)
    {
        UE_LOG(LogSpaceExplorer, Error, TEXT("Galaxy subsystem missing; no planet will be generated."));
        return;
    }
    const FSEStarSystemData Sys = Galaxy->GetCurrentSystem();
    const FSEPlanetData Planet = Galaxy->GetCurrentPlanet();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Streamer = GetWorld()->SpawnActor<ASETerrainStreamer>(ASETerrainStreamer::StaticClass(), FTransform::Identity, Params);
    if (!Streamer) return;
    Streamer->InitForPlanet(Planet);

    Environment = GetWorld()->SpawnActor<ASEEnvironment>(ASEEnvironment::StaticClass(), FTransform::Identity, Params);
    if (Environment) Environment->Init(Planet, Sys.StarColor, Streamer);

    Populator = GetWorld()->SpawnActor<ASEPopulator>(ASEPopulator::StaticClass(), FTransform::Identity, Params);
    if (Populator) Populator->Init(Streamer, Planet, Galaxy->CurrentSystem, Galaxy->CurrentPlanet);
}

void ASEGameMode::StartPlay()
{
    Super::StartPlay();
    UGameInstance* GI = GetGameInstance();
    USEGalaxySubsystem* Galaxy = GI->GetSubsystem<USEGalaxySubsystem>();
    USEProgression* Prog = GI->GetSubsystem<USEProgression>();

    if (Streamer)
    {
        const FVector2D Start = Streamer->FindDrySpot(FVector2D::ZeroVector, 0.f);
        const FVector2D Spot = Streamer->FindDrySpot(Start, 2500.f);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        PlayerShip = GetWorld()->SpawnActor<ASEShip>(ASEShip::StaticClass(), FVector(Spot.X, Spot.Y, Streamer->GetHeightAt(Spot.X, Spot.Y) + 70.f), FRotator::ZeroRotator, Params);
        ASEBaseStructure::SpawnAllBuilt(GetWorld());
    }

    if (Galaxy && Prog)
    {
        const FSEPlanetData P = Galaxy->GetCurrentPlanet();
        if (Prog->AddDiscovery(FName(*FString::Printf(TEXT("planet:%d:%d"), Galaxy->CurrentSystem, Galaxy->CurrentPlanet)), P.Name,
            FString::Printf(TEXT("%.0f C, %.2f g, %s"), P.TemperatureC, P.Gravity, P.bBreathable ? TEXT("breathable air") : TEXT("no breathable air")), 20))
        {
            Prog->PushNotice(FString::Printf(TEXT("New world charted: %s (+20 research)"), *P.Name));
        }
    }

    if (USESaveSubsystem* Save = GI->GetSubsystem<USESaveSubsystem>())
    {
        GetWorldTimerManager().SetTimer(PendingLoadTimer, FTimerDelegate::CreateWeakLambda(this, [Save]() { Save->CompletePendingLoad(); }), 1.0f, false);
        GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this, Save, Prog]()
        {
            ASEPlayerController* PC = Cast<ASEPlayerController>(GetWorld()->GetFirstPlayerController());
            if (Prog && Prog->Settings.bAutosave && PC && PC->Screen == ESEScreen::None)
            {
                if (Save->SaveToSlot(TEXT("auto"))) Prog->PushNotice(TEXT("Autosaved"));
            }
        }), AutosaveSeconds, true);
    }
}
