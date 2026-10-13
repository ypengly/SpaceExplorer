#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

/** Procedural placeholder visuals built from engine primitives (no art assets needed). */
namespace SEVisual
{
    inline UStaticMesh* Basic(const TCHAR* Name)
    {
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
    }
    inline UMaterialInstanceDynamic* Colorize(UPrimitiveComponent* C, const FLinearColor& Col)
    {
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!Base || !C) return nullptr;
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
        MID->SetVectorParameterValue(TEXT("Color"), Col);
        C->SetMaterial(0, MID);
        return MID;
    }
    inline void Setup(UStaticMeshComponent* C, const TCHAR* Mesh, const FLinearColor& Col, const FVector& Scale)
    {
        C->SetStaticMesh(Basic(Mesh));
        C->SetRelativeScale3D(Scale);
        Colorize(C, Col);
    }
    /** Creates, registers and attaches a mesh component at runtime. */
    inline UStaticMeshComponent* AddMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Mesh, const FLinearColor& Col, const FVector& Rel, const FVector& Scale, bool bCollide = true)
    {
        UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
        C->SetMobility(EComponentMobility::Movable);
        C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
        C->RegisterComponent();
        C->SetRelativeLocation(Rel);
        Setup(C, Mesh, Col, Scale);
        C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
        return C;
    }
}
