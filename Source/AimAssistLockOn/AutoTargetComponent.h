#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AutoTargetComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AIMASSISTLOCKON_API UAutoTargetComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UAutoTargetComponent();

    UFUNCTION(BlueprintCallable, Category = "Auto Target")
    AActor* GetCurrentTarget() const;

    UFUNCTION(BlueprintCallable, Category = "Auto Target")
    void ClearCurrentTarget();

    UFUNCTION(BlueprintPure, Category = "Auto Target")
    bool HasTarget() const;

protected:

    virtual void BeginPlay() override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

private:

    void FindBestTarget();

    bool IsValidTarget(AActor* Target) const;

    bool ProjectTargetToScreen(
        AActor* Target,
        FVector2D& OutScreenPosition
    ) const;

    bool HasLineOfSightToTarget(AActor* Target) const;

    float GetScreenDistanceSquared(
        const FVector2D& ScreenPosition
    ) const;

    void UpdateTarget();

    /*
     * Returns the effective targeting radius based
     * on the actual scaled Capsule Component.
     */
    float GetEffectiveTargetingRadius() const;

private:

    /*
     * Base auto-aim range.
     *
     * Keep this at 2000.
     *
     * The value is automatically scaled according
     * to the Blueprint capsule size.
     */
    UPROPERTY(EditAnywhere, Category = "Auto Target")
    float TargetingRadius = 2000.0f;

    /*
     * Radius around the center of the screen.
     *
     * This is screen-space and is NOT scaled with
     * the player's capsule.
     */
    UPROPERTY(EditAnywhere, Category = "Auto Target")
    float ScreenTargetRadius = 1000.0f;

    /*
     * Target update interval.
     *
     * 0 = every frame.
     */
    UPROPERTY(EditAnywhere, Category = "Auto Target")
    float TargetUpdateInterval = 0.0f;

    /*
     * Requires LOS to target players.
     */
    UPROPERTY(EditAnywhere, Category = "Auto Target")
    bool bRequireLineOfSight = true;

    /*
     * Draw the actual effective targeting sphere.
     */
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDrawDebugSphere = false;

    /*
     * Draw a line to the current target.
     */
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDrawDebugTargetLine = false;

    /*
     * Current target.
     */
    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget = nullptr;

    float TimeSinceLastUpdate = 0.0f;
};
