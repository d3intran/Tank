#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;

UCLASS()
class TANK_API ATankPawn : public APawn
{
	GENERATED_BODY()

public:
	ATankPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> TracksMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USceneComponent> GunPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UCameraComponent> Camera;

	// ==========================================
	// Movement Parameters
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s"))
	float TurnSpeed = 60.0f;

	// ==========================================
	// Turret & Gun Parameters
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Turret", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s"))
	float TurretRotateSpeed = 45.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Turret")
	float CurrentTurretYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s"))
	float PitchSpeed = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "-45.0", ClampMax = "0.0", UIMin = "-30.0", UIMax = "0.0"))
	float MinPitch = -5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.0", ClampMax = "85.0", UIMin = "0.0", UIMax = "45.0"))
	float MaxPitch = 25.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Gun")
	float CurrentPitch = 0.0f;

	// ==========================================
	// Visual & Material Parameters
	// ==========================================
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrackMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation")
	FName TrackOffsetParamName = FName(TEXT("TrackOffset"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation", meta = (ClampMin = "1.0", UIMin = "10.0"))
	float TrackUVSpeedScale = 300.0f;

	float TrackUVOffset = 0.0f;

public:
	// Getters for external systems / UI
	FORCEINLINE float GetCurrentTurretYaw() const { return CurrentTurretYaw; }
	FORCEINLINE float GetCurrentPitch() const { return CurrentPitch; }
	FORCEINLINE float GetCurrentMoveSpeed() const { return MoveSpeed; }

private:
	// Axis Inputs
	void MoveForwardInput(float Value);
	void TurnInput(float Value);
	void TurretRotateInput(float Value);
	void PitchUpInput(float Value);

	float CurrentMoveInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float CurrentTurretRotateInput = 0.0f;
	float CurrentPitchInput = 0.0f;
};
