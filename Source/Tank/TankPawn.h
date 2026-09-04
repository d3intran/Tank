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

	// Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement")
	float MoveSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement")
	float TurnSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	float TurretRotateSpeed = 45.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Gun")
	float CurrentTurretYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	float PitchSpeed = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	float MinPitch = -5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	float MaxPitch = 25.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Gun")
	float CurrentPitch = 0.0f;

	// Dynamic Material for Track UV Animation
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrackMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Animation")
	FName TrackOffsetParamName = FName("TrackOffset");

	float TrackUVOffset = 0.0f;

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
