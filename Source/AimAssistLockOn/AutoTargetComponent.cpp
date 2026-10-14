#include "AutoTargetComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"

UAutoTargetComponent::UAutoTargetComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UAutoTargetComponent::BeginPlay()
{
    Super::BeginPlay();

    TimeSinceLastUpdate = TargetUpdateInterval;
}

void UAutoTargetComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(
        DeltaTime,
        TickType,
        ThisTickFunction
    );

    TimeSinceLastUpdate += DeltaTime;

    if (TimeSinceLastUpdate >= TargetUpdateInterval)
    {
        TimeSinceLastUpdate = 0.0f;

        UpdateTarget();
    }

    if (bDrawDebugTargetLine && IsValid(CurrentTarget))
    {
        DrawDebugLine(
            GetWorld(),
            GetOwner()->GetActorLocation(),
            CurrentTarget->GetActorLocation(),
            FColor::Red,
            false,
            -1.0f,
            0,
            3.0f
        );
    }
}

void UAutoTargetComponent::UpdateTarget()
{
    FindBestTarget();
}

void UAutoTargetComponent::FindBestTarget()
{
    AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        ClearCurrentTarget();
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        ClearCurrentTarget();
        return;
    }

    const FVector OwnerLocation =
        Owner->GetActorLocation();

    /*
     * IMPORTANT:
     *
     * This uses the actual Blueprint capsule scale,
     * rather than Actor Scale.
     */
    const float EffectiveTargetingRadius =
        GetEffectiveTargetingRadius();

    if (bDrawDebugSphere)
    {
        DrawDebugSphere(
            World,
            OwnerLocation,
            EffectiveTargetingRadius,
            32,
            FColor::Green,
            false,
            0.0f,
            0,
            2.0f
        );
    }

    /*
     * Use the scaled targeting radius for the overlap.
     */
    const FCollisionShape Sphere =
        FCollisionShape::MakeSphere(
            EffectiveTargetingRadius
        );

    TArray<FOverlapResult> Overlaps;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(AutoTargetOverlap),
        false
    );

    QueryParams.AddIgnoredActor(Owner);

    const bool bFoundTargets =
        World->OverlapMultiByChannel(
            Overlaps,
            OwnerLocation,
            FQuat::Identity,
            ECC_Pawn,
            Sphere,
            QueryParams
        );

    if (!bFoundTargets)
    {
        ClearCurrentTarget();
        return;
    }

    AActor* BestTarget = nullptr;

    float BestScreenDistanceSquared =
        TNumericLimits<float>::Max();

    const float ScreenRadiusSquared =
        FMath::Square(ScreenTargetRadius);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Target =
            Overlap.GetActor();

        if (!IsValidTarget(Target))
        {
            continue;
        }

        FVector2D ScreenPosition;

        if (!ProjectTargetToScreen(
            Target,
            ScreenPosition))
        {
            continue;
        }

        const float ScreenDistanceSquared =
            GetScreenDistanceSquared(
                ScreenPosition
            );

        if (ScreenDistanceSquared >
            ScreenRadiusSquared)
        {
            continue;
        }

        if (ScreenDistanceSquared <
            BestScreenDistanceSquared)
        {
            BestScreenDistanceSquared =
                ScreenDistanceSquared;

            BestTarget = Target;
        }
    }

    if (BestTarget != CurrentTarget)
    {
        CurrentTarget = BestTarget;
    }
}

float UAutoTargetComponent::GetEffectiveTargetingRadius() const
{
    const AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        return TargetingRadius;
    }

    /*
     * Find the Capsule Component used by the Blueprint.
     */
    const UCapsuleComponent* Capsule =
        Owner->FindComponentByClass<UCapsuleComponent>();

    if (!Capsule)
    {
        /*
         * No capsule found.
         *
         * Fall back to the original radius.
         */
        return TargetingRadius;
    }

    /*
     * Get the capsule's original dimensions.
     */
    const float UnscaledRadius =
        Capsule->GetUnscaledCapsuleRadius();

    const float UnscaledHalfHeight =
        Capsule->GetUnscaledCapsuleHalfHeight();

    /*
     * Get the actual dimensions after Blueprint/component
     * scaling has been applied.
     */
    const float ScaledRadius =
        Capsule->GetScaledCapsuleRadius();

    const float ScaledHalfHeight =
        Capsule->GetScaledCapsuleHalfHeight();

    /*
     * Calculate the scale on each capsule dimension.
     */
    float RadiusScale = 1.0f;
    float HeightScale = 1.0f;

    if (UnscaledRadius > KINDA_SMALL_NUMBER)
    {
        RadiusScale =
            ScaledRadius / UnscaledRadius;
    }

    if (UnscaledHalfHeight > KINDA_SMALL_NUMBER)
    {
        HeightScale =
            ScaledHalfHeight / UnscaledHalfHeight;
    }

    /*
     * Use the largest scale.
     *
     * For a capsule scaled uniformly by 10:
     *
     * RadiusScale = 10
     * HeightScale = 10
     *
     * Effective radius = 2000 * 10
     *                   = 20000 cm
     */
    const float CapsuleScale =
        FMath::Max(
            RadiusScale,
            HeightScale
        );

    return TargetingRadius *
        FMath::Max(
            CapsuleScale,
            KINDA_SMALL_NUMBER
        );
}

bool UAutoTargetComponent::IsValidTarget(
    AActor* Target
) const
{
    if (!IsValid(Target))
    {
        return false;
    }

    if (Target == GetOwner())
    {
        return false;
    }

    if (!Target->IsA<APawn>())
    {
        return false;
    }

    if (bRequireLineOfSight)
    {
        if (!HasLineOfSightToTarget(Target))
        {
            return false;
        }
    }

    return true;
}

bool UAutoTargetComponent::ProjectTargetToScreen(
    AActor* Target,
    FVector2D& OutScreenPosition
) const
{
    if (!IsValid(Target))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
    }

    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!PlayerController)
    {
        return false;
    }

    /*
     * Actor location is already in world space.
     *
     * Do NOT multiply this location by the capsule scale.
     */
    const FVector WorldLocation =
        Target->GetActorLocation();

    return PlayerController->ProjectWorldLocationToScreen(
        WorldLocation,
        OutScreenPosition,
        true
    );
}

bool UAutoTargetComponent::HasLineOfSightToTarget(
    AActor* Target
) const
{
    if (!IsValid(Target))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
    }

    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!PlayerController)
    {
        return false;
    }

    FVector CameraLocation;
    FRotator CameraRotation;

    PlayerController->GetPlayerViewPoint(
        CameraLocation,
        CameraRotation
    );

    const FVector TargetLocation =
        Target->GetActorLocation();

    FHitResult HitResult;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(AutoTargetLOS),
        true
    );

    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.AddIgnoredActor(Target);

    const bool bBlocked =
        World->LineTraceSingleByChannel(
            HitResult,
            CameraLocation,
            TargetLocation,
            ECC_Visibility,
            QueryParams
        );

    if (!bBlocked)
    {
        return true;
    }

    return HitResult.GetActor() == Target;
}

float UAutoTargetComponent::GetScreenDistanceSquared(
    const FVector2D& ScreenPosition
) const
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return TNumericLimits<float>::Max();
    }

    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!PlayerController)
    {
        return TNumericLimits<float>::Max();
    }

    int32 ViewportSizeX = 0;
    int32 ViewportSizeY = 0;

    PlayerController->GetViewportSize(
        ViewportSizeX,
        ViewportSizeY
    );

    const FVector2D ScreenCenter(
        static_cast<float>(ViewportSizeX) * 0.5f,
        static_cast<float>(ViewportSizeY) * 0.5f
    );

    return FVector2D::DistSquared(
        ScreenPosition,
        ScreenCenter
    );
}

AActor* UAutoTargetComponent::GetCurrentTarget() const
{
    return CurrentTarget;
}

void UAutoTargetComponent::ClearCurrentTarget()
{
    CurrentTarget = nullptr;
}

bool UAutoTargetComponent::HasTarget() const
{
    return IsValid(CurrentTarget);
}
