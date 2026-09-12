#include "TankVehicle.h"

#include "BattleGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Tank.h"
#include "TankHealth.h"
#include "TankPlayerController.h"
#include "TankProjectile.h"

// ============================================================================
// 炮塔/火炮伺服与 3D 瞄准落点探测
// ============================================================================
void ATankVehicle::UpdateTurretVisuals(float DeltaTime)
{
	// 炮塔/火炮朝向：本机受控 → 输入驱动伺服算权威值；远端 → 直接吃复制值
	if (IsLocallyControlled())
	{
		if (!FMath::IsNearlyZero(CurrentTurretRotateInput))
		{
			CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + CurrentTurretRotateInput * TurretRotateSpeed * DeltaTime);
		}
		CurrentTurretYaw = FMath::FixedTurn(CurrentTurretYaw, CameraRelativeYaw, TurretRotateSpeed * DeltaTime);
		CurrentPitch = FMath::FInterpConstantTo(CurrentPitch, DesiredGunPitch, DeltaTime, PitchSpeed);
	}
	else
	{
		CurrentTurretYaw = FMath::FixedTurn(CurrentTurretYaw, NetTurretYaw, TurretRotateSpeed * DeltaTime);
		CurrentPitch = FMath::FInterpConstantTo(CurrentPitch, NetGunPitch, DeltaTime, PitchSpeed);
	}

	// 主机自己的车：把伺服结果写进复制字段
	if (HasAuthority() && IsLocallyControlled())
	{
		NetTurretYaw = CurrentTurretYaw;
		NetGunPitch = CurrentPitch;
	}

	if (TurretPivot)
	{
		TurretPivot->SetRelativeRotation(FRotator(0.0f, CurrentTurretYaw, 0.0f));
	}
	if (GunPivot)
	{
		GunPivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}

	// 后坐力复位
	if (GunMesh)
	{
		if (CurrentRecoilOffset < -0.01f)
		{
			CurrentRecoilOffset = FMath::FInterpTo(CurrentRecoilOffset, 0.0f, DeltaTime, RecoilRecoverySpeed);
		}
		else
		{
			CurrentRecoilOffset = 0.0f;
		}
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 视口跟随
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}
	if (bEnableAutoCenter && !FMath::IsNearlyZero(CurrentThrottleInput))
	{
		CameraRelativeYaw = FMath::FInterpTo(CameraRelativeYaw, 0.0f, DeltaTime, AutoCenterSpeed);
	}

	// 瞄准落点与目标检测
	if (GunMesh && GetWorld())
	{
		const FVector MuzzleLoc = GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f));
		const FVector GunForward = GunMesh->GetForwardVector();
		const FVector TraceEnd = MuzzleLoc + GunForward * MaxAimDistance;

		FHitResult GunHit;
		FCollisionQueryParams GunQueryParams(TEXT("TankVehicleGunTrace"), /*bTraceComplex=*/false, this);
		GunQueryParams.AddIgnoredActor(this);

		CachedAimResult.MuzzleLocation = MuzzleLoc;
		if (GetWorld()->LineTraceSingleByChannel(GunHit, MuzzleLoc, TraceEnd, ECC_Visibility, GunQueryParams))
		{
			CachedAimResult.bHit = true;
			CachedAimResult.AimPoint = GunHit.ImpactPoint;
			CachedAimResult.AimNormal = GunHit.ImpactNormal;
			CachedAimResult.AimDistance = GunHit.Distance;
			CachedAimResult.HitActor = GunHit.GetActor();

			AActor* Target = GunHit.GetActor();
			CachedAimResult.bLockedOnEnemy = (Target != nullptr && Target != this && Target->IsA<APawn>());
		}
		else
		{
			CachedAimResult.bHit = false;
			CachedAimResult.AimPoint = TraceEnd;
			CachedAimResult.AimNormal = -GunForward;
			CachedAimResult.AimDistance = MaxAimDistance;
			CachedAimResult.bLockedOnEnemy = false;
			CachedAimResult.HitActor = nullptr;
		}

		// 3D 物理与激光辅助线（仅本地受控车辆绘制，避免远端视口污染）
		if (bDrawAimDebug && IsLocallyControlled())
		{
			const FColor LaserColor = CachedAimResult.bLockedOnEnemy ? FColor(255, 30, 30) : FColor(255, 200, 40);
			const FColor RingColor = CachedAimResult.bLockedOnEnemy ? FColor(255, 40, 40) : FColor(70, 210, 255);

			// 炮口激光束
			DrawDebugLine(GetWorld(), MuzzleLoc, CachedAimResult.bHit ? CachedAimResult.AimPoint : (MuzzleLoc + GunForward * 3000.0f),
				LaserColor, false, -1.0f, 0, 1.8f);

			if (CachedAimResult.bHit)
			{
				// 贴合碰撞表面法线的准心圆环
				const FMatrix RingTM = FRotationMatrix::MakeFromZ(CachedAimResult.AimNormal) * FTranslationMatrix(CachedAimResult.AimPoint + CachedAimResult.AimNormal * 2.0f);
				DrawDebugCircle(GetWorld(), RingTM, 45.0f, 24, RingColor, false, -1.0f, 0, 2.5f);
				DrawDebugPoint(GetWorld(), CachedAimResult.AimPoint + CachedAimResult.AimNormal * 2.0f, 8.0f, RingColor, false, -1.0f, 0);
			}
		}
	}
}

// ============================================================================
// 开火流程与弹道派发
// ============================================================================
void ATankVehicle::Fire()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (TankHealth && TankHealth->IsDepleted())
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireCooldown)
	{
		return;
	}
	LastFireTime = CurrentTime;

	// 后坐力（本机即时）
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	const FVector MuzzleLoc = GunMesh
		? GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f))
		: GetActorLocation();
	const FVector AimDir = GunMesh ? GunMesh->GetComponentRotation().Vector() : GetActorRotation().Vector();

	if (HasAuthority())
	{
		ExecuteFire(MuzzleLoc, AimDir);
		MulticastFireFX(MuzzleLoc, AimDir);
	}
	else
	{
		ServerFire(MuzzleLoc, AimDir);
	}

	DrawDebugLine(World, MuzzleLoc, MuzzleLoc + AimDir * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
}

void ATankVehicle::ExecuteFire(const FVector& MuzzleLoc, const FVector& AimDir)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ATankProjectile::StaticClass();
	World->SpawnActor<ATankProjectile>(ClassToSpawn, MuzzleLoc, AimDir.Rotation(), SpawnParams);

	UE_LOG(LogTank, Log, TEXT("[Vehicle] %s 开火 @ (%.0f,%.0f,%.0f)"), *GetName(), MuzzleLoc.X, MuzzleLoc.Y, MuzzleLoc.Z);
}

bool ATankVehicle::ServerFire_Validate(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	const FVector Loc(MuzzleLoc);
	const FVector Dir(AimDir);
	return !Loc.ContainsNaN() && !Dir.ContainsNaN() && !Dir.IsNearlyZero();
}

void ATankVehicle::ServerFire_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown * 0.5f)
	{
		return;
	}
	LastFireTime = Now;

	ExecuteFire(FVector(MuzzleLoc), FVector(AimDir));
	MulticastFireFX(MuzzleLoc, AimDir);
}

void ATankVehicle::MulticastFireFX_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, MuzzleLoc, FVector(MuzzleLoc) + FVector(AimDir) * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
	}
}

// ============================================================================
// 伤害受击与阵亡处理
// ============================================================================
float ATankVehicle::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || DamageAmount <= 0.0f || !TankHealth || TankHealth->IsDepleted())
	{
		return 0.0f;
	}

	if (bSpawnProtected)
	{
		return 0.0f;
	}

	const float Applied = TankHealth->ApplyDamage(DamageAmount);
	if (TankHealth->IsDepleted())
	{
		HandleDeath(EventInstigator);
	}
	return Applied;
}

void ATankVehicle::HandleDeath(AController* Killer)
{
	// 规则判定归 GameMode，这里只上报「谁杀了谁」（必须先于销毁上报）
	if (ABattleGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABattleGameMode>() : nullptr)
	{
		GameMode->NotifyKill(Killer, GetController());
	}

	const FVector DeathLoc = GetActorLocation();
	MulticastDeathFX(DeathLoc);

	// 让被击杀玩家的镜头停留在残骸后上方回看（避免镜头掉落原点朝天）
	const FRotator DeathRot = GetActorRotation();
	const FVector DeathCamLoc = DeathLoc - DeathRot.Vector() * 900.0f + FVector(0.0f, 0.0f, 350.0f);

	if (APlayerController* OwnerPC = Cast<APlayerController>(GetController()))
	{
		if (OwnerPC->IsLocalController())
		{
			ClientSetDeathCamera_Implementation(DeathCamLoc, DeathRot);
		}
		else
		{
			ClientSetDeathCamera(DeathCamLoc, DeathRot);
		}
	}

	// 留出 2s 爆炸与观战窗口；重生由 GameMode 的占有巡检补发新坦克
	SetLifeSpan(2.0f);
}

void ATankVehicle::MulticastDeathFX_Implementation(FVector_NetQuantize100 DeathLoc)
{
	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, FVector(DeathLoc), 250.0f, 16, FColor::Orange, false, 2.0f, 0, 6.0f);
		DrawDebugSphere(World, FVector(DeathLoc) + FVector(0, 0, 120.0f), 150.0f, 16, FColor::Red, false, 2.0f, 0, 4.0f);
	}
}

void ATankVehicle::ClientSetDeathCamera_Implementation(FVector_NetQuantize100 Location, FRotator Rotation)
{
	if (ATankPlayerController* PC = Cast<ATankPlayerController>(GetController()))
	{
		PC->SetDeathViewLocation(FVector(Location), Rotation);
	}
}

void ATankVehicle::OnRep_SpawnProtected()
{
	// 客户端表现钩子（重生保护提示由 HUD 读 bSpawnProtected 显示）
}

void ATankVehicle::ClearSpawnProtection()
{
	bSpawnProtected = false;
	OnRep_SpawnProtected();
}
