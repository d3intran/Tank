#include "Zombie.h"
#include "Tank.h"
#include "TankPawn.h"
#include "ClimbManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

AZombie::AZombie()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetCapsuleRadius(40.0f);
	Capsule->SetCapsuleHalfHeight(90.0f);
	Capsule->SetCollisionObjectType(ECC_GameTraceChannel1);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	RootComponent = Capsule;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshAsset.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterialAsset.Succeeded())
	{
		BodyMesh->SetMaterial(0, ShapeMaterialAsset.Object);
	}
}

void AZombie::BeginPlay()
{
	Super::BeginPlay();

	GroundZ = GetActorLocation().Z;
	BaseOriginZ = GroundZ;

	BodyMaterialInstance = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (BodyMaterialInstance)
	{
		BodyMaterialInstance->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.45f, 0.15f, 1.0f));
	}

	if (TargetWall.IsValid())
	{
		CachedClimbManager = TargetWall->FindComponentByClass<UClimbManager>();
		if (CachedClimbManager.IsValid())
		{
			CachedClimbManager->EnsureOrientationFrom(GetActorLocation());
			PileCenter = CachedClimbManager->GetPileCenter();
			PileCenter.Z = 0.0f;
		}
		else
		{
			UE_LOG(LogTank, Warning, TEXT("[Zombie] %s 的目标墙上无 ClimbManager"), *GetName());
		}
	}
	else
	{
		UE_LOG(LogTank, Warning, TEXT("[Zombie] %s 无目标墙，原地站立"), *GetName());
	}

	for (TActorIterator<ATankPawn> It(GetWorld()); It; ++It)
	{
		CachedTank = *It;
		break;
	}
}

void AZombie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (State)
	{
	case EZombieState::WalkToWall:
	{
		if (IsCrushedByTank())
		{
			FVector Radial = GetActorLocation() - CachedTank->GetActorLocation();
			Radial.Z = 0.0f;
			Die(true, Radial.IsNearlyZero() ? CachedTank->GetActorForwardVector() : Radial.GetSafeNormal());
			return;
		}

		if (!CachedClimbManager.IsValid())
		{
			return;
		}

		const float DistToCenter = FVector::Dist2D(GetActorLocation(), PileCenter);
		if (DistToCenter <= CachedClimbManager->GetPileRadius() + 120.0f)
		{
			AssignedSlot = CachedClimbManager->AcquireSlot();
			if (AssignedSlot != INDEX_NONE)
			{
				SlotTarget = CachedClimbManager->GetSlotLocation(AssignedSlot);
				State = EZombieState::AtPile;
				UE_LOG(LogTank, Log, TEXT("[Zombie] %s 领到槽位 %d → (%.0f,%.0f,%.0f)"),
					*GetName(), AssignedSlot, SlotTarget.X, SlotTarget.Y, SlotTarget.Z);
			}
			else if (CachedClimbManager->IsPileFull())
			{
				State = EZombieState::Climbing;
				UE_LOG(LogTank, Log, TEXT("[Zombie] %s 槽满，开始攀爬"), *GetName());
			}
			break;
		}

		GroundStepTo(PileCenter, DeltaTime, bWounded ? WoundedSpeedScale : 1.0f);
		SetActorLocation(GetActorLocation() + ComputeSeparationOffset(DeltaTime), false);
		break;
	}
	case EZombieState::AtPile:
	{
		if (IsCrushedByTank())
		{
			if (AssignedSlot != INDEX_NONE && CachedClimbManager.IsValid())
			{
				CachedClimbManager->ReleaseSlot(AssignedSlot);
				AssignedSlot = INDEX_NONE;
			}
			FVector Radial = GetActorLocation() - CachedTank->GetActorLocation();
			Radial.Z = 0.0f;
			Die(true, Radial.IsNearlyZero() ? CachedTank->GetActorForwardVector() : Radial.GetSafeNormal());
			return;
		}

		const float Speed = SlotLerpSpeed * (bWounded ? WoundedSpeedScale : 1.0f);
		FVector Current = GetActorLocation();
		FVector NewLoc = FMath::VInterpTo(Current, SlotTarget, DeltaTime, Speed / FMath::Max(FVector::Dist(Current, SlotTarget), 1.0f));

		if (FVector::Dist(NewLoc, SlotTarget) < 5.0f)
		{
			NewLoc = SlotTarget;
		}
		FVector SepOffset = ComputeSeparationOffset(DeltaTime);
		NewLoc.X += SepOffset.X;
		NewLoc.Y += SepOffset.Y;
		SetActorLocation(NewLoc, false);

		FVector FaceDir = SlotTarget - PileCenter;
		FaceDir.Z = 0.0f;
		if (!FaceDir.IsNearlyZero())
		{
			UpdateFacing(-FaceDir.GetSafeNormal(), DeltaTime);
		}
		break;
	}
	case EZombieState::Climbing:
	{
		if (IsCrushedByTank())
		{
			FVector Radial = GetActorLocation() - CachedTank->GetActorLocation();
			Radial.Z = 0.0f;
			Die(true, Radial.IsNearlyZero() ? CachedTank->GetActorForwardVector() : Radial.GetSafeNormal());
			return;
		}

		const float Speed = ClimbSpeed * (bWounded ? WoundedSpeedScale : 1.0f);
		FVector P = GetActorLocation();
		P.Z += Speed * DeltaTime;

		if (CachedClimbManager.IsValid() && P.Z >= CachedClimbManager->GetWallTopZ())
		{
			VanishAtTop();
			return;
		}

		FVector ToWall = PileCenter - P;
		ToWall.Z = 0.0f;
		if (!ToWall.IsNearlyZero() && ToWall.Size2D() > 30.0f)
		{
			P += ToWall.GetSafeNormal() * 20.0f * DeltaTime;
		}

		SetActorLocation(P, false);
		UpdateFacing(CachedClimbManager.IsValid() ? -CachedClimbManager->GetOutwardDir() : FVector::XAxisVector, DeltaTime);
		break;
	}
	case EZombieState::Trampled:
	{
		if (IsCrushedByTank())
		{
			if (AssignedSlot != INDEX_NONE && CachedClimbManager.IsValid())
			{
				CachedClimbManager->ReleaseSlot(AssignedSlot);
				AssignedSlot = INDEX_NONE;
			}
			FVector Radial = GetActorLocation() - CachedTank->GetActorLocation();
			Radial.Z = 0.0f;
			Die(true, Radial.IsNearlyZero() ? CachedTank->GetActorForwardVector() : Radial.GetSafeNormal());
			return;
		}
		break;
	}
	case EZombieState::Dead:
	{
		if (!bCorpseSettled)
		{
			FVector P = GetActorLocation() + DeadVelocity * DeltaTime;
			DeadVelocity.Z -= 2000.0f * DeltaTime;
			if (P.Z <= GroundZ)
			{
				P.Z = GroundZ;
				bCorpseSettled = true;
			}
			SetActorLocation(P, true);
		}
		else
		{
			const float S = FMath::Max(GetActorScale3D().X - DeltaTime * 6.0f, 0.01f);
			SetActorScale3D(FVector(S));
			if (S <= 0.05f)
			{
				Destroy();
			}
		}
		break;
	}
	}
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (State == EZombieState::Dead)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health -= ActualDamage;

	FVector FlingDir = FVector::XAxisVector;
	if (DamageCauser)
	{
		FlingDir = GetActorLocation() - DamageCauser->GetActorLocation();
		FlingDir.Z = 0.0f;
		FlingDir = FlingDir.IsNearlyZero() ? FVector::XAxisVector : FlingDir.GetSafeNormal();
	}

	if (Health <= 0.0f)
	{
		if (AssignedSlot != INDEX_NONE && CachedClimbManager.IsValid())
		{
			CachedClimbManager->ReleaseSlot(AssignedSlot);
			AssignedSlot = INDEX_NONE;
		}
		Die(false, FlingDir);
	}
	else if (!bWounded)
	{
		bWounded = true;
		if (State != EZombieState::Trampled)
		{
			BodyMesh->SetRelativeRotation(FRotator(-65.0f, 0.0f, 0.0f));
		}
		if (BodyMaterialInstance)
		{
			BodyMaterialInstance->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.22f, 0.10f, 1.0f));
		}
		UE_LOG(LogTank, Log, TEXT("[Zombie] %s 被打残（余 %.0f HP）"), *GetName(), Health);
	}
	return ActualDamage;
}

void AZombie::GroundStepTo(const FVector& Target, float DeltaTime, float SpeedScale)
{
	FVector MoveDir = Target - GetActorLocation();
	MoveDir.Z = 0.0f;
	if (MoveDir.IsNearlyZero())
	{
		return;
	}
	MoveDir.Normalize();
	UpdateFacing(MoveDir, DeltaTime);
	FVector Offset = MoveDir * MoveSpeed * SpeedScale * DeltaTime;
	Offset.Z = 0.0f;
	SetActorLocation(GetActorLocation() + Offset, true);
}

void AZombie::LieDown()
{
	BodyMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 87.0f));
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -55.0f));
}

void AZombie::Die(bool bCrushed, const FVector& FlingDir)
{
	if (State == EZombieState::Dead)
	{
		return;
	}
	State = EZombieState::Dead;

	GroundZ = FMath::Min(GroundZ, GetActorLocation().Z);
	const float HorizontalSpeed = bCrushed ? 2200.0f : 2800.0f;
	DeadVelocity = FlingDir * HorizontalSpeed + FVector(0.0f, 0.0f, 600.0f);
	bCorpseSettled = false;

	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, GetActorLocation() + FVector(0, 0, 60.0f), 50.0f, 12,
			bCrushed ? FColor::Yellow : FColor::Red, false, 1.5f, 0, 2.0f);
	}
	UE_LOG(LogTank, Log, TEXT("[Zombie] %s 死亡（%s）"), *GetName(), bCrushed ? TEXT("碾压") : TEXT("炮击"));
}

void AZombie::VanishAtTop()
{
	if (State == EZombieState::Dead)
	{
		return;
	}
	if (CachedClimbManager.IsValid())
	{
		CachedClimbManager->NotifyTopOut(this);
	}
	State = EZombieState::Dead;
	bCorpseSettled = true;
	UE_LOG(LogTank, Log, TEXT("[Zombie] %s 登顶消失"), *GetName());
}

void AZombie::UpdateFacing(const FVector& MoveDir, float DeltaTime)
{
	const FRotator TargetYaw(0.0f, MoveDir.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetYaw, DeltaTime, TurnSpeed));
}

bool AZombie::IsCrushedByTank() const
{
	if (!CachedTank.IsValid() || GetActorLocation().Z > CachedTank->GetActorLocation().Z + 300.0f)
	{
		return false;
	}
	return FVector::Dist2D(GetActorLocation(), CachedTank->GetActorLocation()) <= CrushRadius;
}

FVector AZombie::ComputeSeparationOffset(float DeltaTime) const
{
	FVector Push = FVector::ZeroVector;
	const FVector MyLoc = GetActorLocation();

	for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->State == EZombieState::Dead)
		{
			continue;
		}
		FVector Delta = MyLoc - It->GetActorLocation();
		Delta.Z = 0.0f;
		const float Dist = Delta.Size();
		if (Dist < KINDA_SMALL_NUMBER || Dist > SeparationRadius)
		{
			continue;
		}
		const float Weight = 1.0f - Dist / SeparationRadius;
		Push += Delta.GetSafeNormal() * (SeparationStrength * Weight);
	}
	return Push * DeltaTime;
}
