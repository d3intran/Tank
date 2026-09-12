#include "TankPlayerController.h"
#include "Tank.h"

ATankPlayerController::ATankPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATankPlayerController::SetDeathViewLocation(const FVector& InLocation, const FRotator& InRotation)
{
	DeathViewLocation = InLocation;
	DeathViewRotation = InRotation;
	bHasDeathView = true;

	UE_LOG(LogTank, Verbose, TEXT("[Battle] %s 阵亡观战位姿设为 (%.0f,%.0f,%.0f)"),
		*GetName(), InLocation.X, InLocation.Y, InLocation.Z);
}

FVector ATankPlayerController::GetFocalLocation() const
{
	// 只在「没有 Pawn（阵亡窗口）且记录过观战位姿」时接管
	if (GetPawn() == nullptr && bHasDeathView)
	{
		return DeathViewLocation;
	}
	return Super::GetFocalLocation();
}

FRotator ATankPlayerController::GetControlRotation() const
{
	if (GetPawn() == nullptr && bHasDeathView)
	{
		return DeathViewRotation;
	}
	return Super::GetControlRotation();
}

void ATankPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 重生后交还给引擎默认（跟随新 Pawn），避免残留的观战位姿影响正常操控
	bHasDeathView = false;
}
