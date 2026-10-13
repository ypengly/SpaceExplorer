#include "SEWalker.h"
#include "SETerrainStreamer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ASEWalker::ASEWalker()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetCollisionProfileName(TEXT("NoCollision"));
    Body->SetMobility(EComponentMobility::Movable);

    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* M = GetCharacterMovement();
    M->bOrientRotationToMovement = true;
    M->RotationRate = FRotator(0.f, 360.f, 0.f);
    M->bRunPhysicsWithNoController = true;
    M->MaxWalkSpeed = WalkSpeed;
}

void ASEWalker::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ASETerrainStreamer> It(GetWorld()); It; ++It) { Streamer = *It; break; }
    GetCharacterMovement()->SetMovementMode(MOVE_None); // until terrain collision exists under us
}

bool ASEWalker::MoveToward(const FVector& Target, float SpeedScale, float Accept)
{
    FVector D = Target - GetActorLocation();
    D.Z = 0.f;
    const float Dist = D.Size();
    if (Dist <= Accept) return true;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * SpeedScale;
    AddMovementInput(D / Dist, 1.f);
    return false;
}

void ASEWalker::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bWaitingTerrain)
    {
        if (!Streamer.IsValid() || Streamer->IsAreaReady(GetActorLocation()))
        {
            bWaitingTerrain = false;
            GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
        return;
    }
    DistTimer -= Dt;
    if (DistTimer <= 0.f)
    {
        DistTimer = 0.5f;
        if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0)) { DistToPlayer = FVector::Dist(P->GetActorLocation(), GetActorLocation()); }
        SetActorTickInterval(DistToPlayer > 20000.f ? 0.5f : (DistToPlayer > 8000.f ? 0.15f : 0.f));
    }
    if (DistToPlayer > 25000.f) return; // far away: simulation frozen
    Think(Dt);
}
