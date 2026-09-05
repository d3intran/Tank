#include "TankPawn.h"
#include "Tank.h"
#include "TankProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根碰撞盒：根据 1:1 真实尺寸（长7.6m, 宽3.5m, 高2.4m）
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(380.0f, 175.0f, 120.0f));
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetSimulatePhysics(false);
	RootComponent = CollisionBox;

	// 2. 车身底盘 Mesh（底部对齐碰撞盒底面 Z=-120）
	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(CollisionBox);
	HullMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -120.0f));
	HullMesh->SetRelativeScale3D(FVector::OneVector);
	HullMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_hull.ztz88a_hull"));
	if (HullMeshAsset.Succeeded())
	{
		HullMesh->SetStaticMesh(HullMeshAsset.Object);
	}

	// 3. 履带 Mesh（与底盘同原点同基准，严丝合缝包裹负重轮）
	TracksMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracksMesh"));
	TracksMesh->SetupAttachment(HullMesh);
	TracksMesh->SetRelativeLocation(FVector::ZeroVector);
	TracksMesh->SetRelativeScale3D(FVector::OneVector);
	TracksMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TracksMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_tracks.ztz88a_tracks"));
	if (TracksMeshAsset.Succeeded())
	{
		TracksMesh->SetStaticMesh(TracksMeshAsset.Object);
	}

	// 4. 炮塔水平旋转轴心 (挂在车身上，坐标：X=-2.5, Y=0, Z=145.5)
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(HullMesh);
	TurretPivot->SetRelativeLocation(FVector(-2.5f, 0.0f, 145.5f));
	TurretPivot->SetRelativeScale3D(FVector::OneVector);

	// 5. 炮塔 Mesh（完整包含铸造炮塔、五角星侧装甲裙框、高射机枪与指挥塔）
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretPivot);
	TurretMesh->SetRelativeLocation(FVector::ZeroVector);
	TurretMesh->SetRelativeRotation(FRotator::ZeroRotator);
	TurretMesh->SetRelativeScale3D(FVector::OneVector);
	TurretMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-turret.ztz88a-turret"));
	if (TurretMeshAsset.Succeeded())
	{
		TurretMesh->SetStaticMesh(TurretMeshAsset.Object);
	}

	// 6. 主炮俯仰轴心 (挂在炮塔轴心上，相对于炮塔基准点坐标：X=170.0, Y=0, Z=31.0)
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(TurretPivot);
	GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
	GunPivot->SetRelativeScale3D(FVector::OneVector);

	// 7. 主炮 Mesh
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GunPivot);
	GunMesh->SetRelativeLocation(FVector::ZeroVector);
	GunMesh->SetRelativeRotation(FRotator::ZeroRotator);
	GunMesh->SetRelativeScale3D(FVector::OneVector);
	GunMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-gun.ztz88a-gun"));
	if (GunMeshAsset.Succeeded())
	{
		GunMesh->SetStaticMesh(GunMeshAsset.Object);
	}

	// 8. 现代商业标杆视口弹簧臂（锁定舒适俯视角，彻底杜绝翻转眩晕，仅绕车体水平回旋）
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = 950.0f;
	SpringArm->SetRelativeLocation(FVector(-50.0f, 0.0f, 220.0f));
	SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true; // 核心修复：继承底盘偏航，WA转向时视口自然跟随车身切入弯道！
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 8.0f; // 优雅的旋转滞后平滑，营造重型载具厚重感

	// 9. 摄像机
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
	ProjectileClass = ATankProjectile::StaticClass();
}

void ATankPawn::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// 核心架构自愈机制：强制保证编辑期与运行期拓扑及相对位姿一致性
	if (TurretPivot && GunPivot)
	{
		GunPivot->AttachToComponent(TurretPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
		GunPivot->SetRelativeScale3D(FVector::OneVector);
	}

	if (GunMesh && GunPivot)
	{
		GunMesh->AttachToComponent(GunPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
		GunMesh->SetRelativeScale3D(FVector::OneVector);
	}
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();

	// 1. 初始化动态履带材质实例
	if (TracksMesh)
	{
		if (TracksMesh->GetNumMaterials() > 0)
		{
			TrackMaterialInstance = TracksMesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		else
		{
			UE_LOG(LogTank, Warning, TEXT("TracksMesh has no materials assigned. Track UV animation disabled."));
		}
	}

	// 2. 初始化相机朝向与火炮仰角
	CameraRelativeYaw = 0.0f;
	DesiredGunPitch = 0.0f;
	CurrentPitch = 0.0f;

	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	UE_LOG(LogTank, Log, TEXT("TankPawn initialized with Vehicle-Relative Camera and Direct Gun Elevation."));
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. WASD 前后行进
	if (!FMath::IsNearlyZero(CurrentMoveInput))
	{
		const FVector MoveDelta = FVector(CurrentMoveInput * MoveSpeed * DeltaTime, 0.0f, 0.0f);
		FHitResult Hit;
		AddActorLocalOffset(MoveDelta, true, &Hit);

		if (TrackMaterialInstance)
		{
			TrackUVOffset += CurrentMoveInput * (MoveSpeed / FMath::Max(1.0f, TrackUVSpeedScale)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 2. WASD 原地差速掉头
	if (!FMath::IsNearlyZero(CurrentTurnInput))
	{
		const FRotator TurnDelta = FRotator(0.0f, CurrentTurnInput * TurnSpeed * DeltaTime, 0.0f);
		AddActorLocalRotation(TurnDelta, true);

		if (TrackMaterialInstance && FMath::IsNearlyZero(CurrentMoveInput))
		{
			TrackUVOffset += CurrentTurnInput * (TurnSpeed / FMath::Max(1.0f, TrackUVSpeedScale * 0.1f)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 3. 视口跟随：继承车身偏航并叠加相对偏航角 CameraRelativeYaw，Pitch锁定在舒适俯角
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	// 4. 直线行驶自动回正（如启用）
	if (bEnableAutoCenter && !FMath::IsNearlyZero(CurrentMoveInput) && FMath::IsNearlyZero(CurrentTurnInput))
	{
		CameraRelativeYaw = FMath::FInterpTo(CameraRelativeYaw, 0.0f, DeltaTime, AutoCenterSpeed);
	}

	// 5. 炮塔水平电驱伺服追踪（追赶玩家视线相对偏航角 CameraRelativeYaw）
	if (!FMath::IsNearlyZero(CurrentTurretRotateInput))
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + CurrentTurretRotateInput * TurretRotateSpeed * DeltaTime);
	}
	CurrentTurretYaw = FMath::FixedTurn(CurrentTurretYaw, CameraRelativeYaw, TurretRotateSpeed * DeltaTime);

	// 5. 主炮垂直电驱高低机平滑追踪（平滑向鼠标纵向指定的 DesiredGunPitch 靠拢）
	if (!FMath::IsNearlyZero(CurrentPitchInput))
	{
		DesiredGunPitch = FMath::Clamp(DesiredGunPitch + CurrentPitchInput * PitchSpeed * DeltaTime, MinPitch, MaxPitch);
	}
	CurrentPitch = FMath::FInterpConstantTo(CurrentPitch, DesiredGunPitch, DeltaTime, PitchSpeed);

	if (TurretPivot)
	{
		TurretPivot->SetRelativeRotation(FRotator(0.0f, CurrentTurretYaw, 0.0f));
	}
	if (GunPivot)
	{
		GunPivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}

	// 6. 主炮后坐力渐进平滑复位
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

	// 7. 真实弹道指向与射击点（从炮口沿炮管轴线探测，鼠标纵向调节时清晰上下位移）
	if (bDrawAimDebug && GetWorld() && GunMesh)
	{
		const FVector MuzzleLoc = GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f));
		const FVector GunForward = GunMesh->GetForwardVector();
		const FVector GunTraceEnd = MuzzleLoc + GunForward * MaxAimDistance;

		FHitResult GunHit;
		FCollisionQueryParams GunQueryParams(TEXT("TankGunTrace"), false, this);
		GunQueryParams.AddIgnoredActor(this);

		FVector ActualAimPoint = GunTraceEnd;
		if (GetWorld()->LineTraceSingleByChannel(GunHit, MuzzleLoc, GunTraceEnd, ECC_Visibility, GunQueryParams))
		{
			ActualAimPoint = GunHit.ImpactPoint;
		}

		// 绘制实际射击着弹点与主炮指向射线
		DrawDebugSphere(GetWorld(), ActualAimPoint, 35.0f, 16, FColor::Yellow, false, -1.0f, 0, 2.5f);
		DrawDebugLine(GetWorld(), MuzzleLoc, MuzzleLoc + GunForward * 1200.0f, FColor::Orange, false, -1.0f, 0, 2.0f);
	}

	CurrentMoveInput = 0.0f;
	CurrentTurnInput = 0.0f;
	CurrentTurretRotateInput = 0.0f;
	CurrentPitchInput = 0.0f;
}

void ATankPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// 基础驱动与差速掉头
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ATankPawn::MoveForwardInput);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ATankPawn::TurnInput);

	// 现代商业标杆视口与射击点控制
	PlayerInputComponent->BindAxis(TEXT("TurnCamera"), this, &ATankPawn::TurnCameraInput);
	PlayerInputComponent->BindAxis(TEXT("LookUpCamera"), this, &ATankPawn::LookUpCameraInput);

	// 主炮开火
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ATankPawn::FireInput);

	// 键盘微调兼容
	PlayerInputComponent->BindAxis(TEXT("TurretRotate"), this, &ATankPawn::TurretRotateInput);
	PlayerInputComponent->BindAxis(TEXT("PitchGun"), this, &ATankPawn::PitchUpInput);
}

void ATankPawn::MoveForwardInput(float Value)
{
	CurrentMoveInput = Value;
}

void ATankPawn::TurnInput(float Value)
{
	CurrentTurnInput = Value;
}

void ATankPawn::TurnCameraInput(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + Value * CameraSensitivityX);
	}
}

void ATankPawn::LookUpCameraInput(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		// 鼠标向上推 -> 仰角增大(向上抬)；鼠标向下拉 -> 仰角减小(向下压)
		const float Direction = bInvertPitch ? -1.0f : 1.0f;
		DesiredGunPitch = FMath::Clamp(DesiredGunPitch + Value * PitchSensitivity * Direction, MinPitch, MaxPitch);
	}
}

void ATankPawn::FireInput()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireCooldown)
	{
		UE_LOG(LogTank, Verbose, TEXT("[Tank] Gun reloading: %.2f / %.2fs"), (CurrentTime - LastFireTime), FireCooldown);
		return;
	}

	LastFireTime = CurrentTime;

	// 1. 触发主炮液压后坐力冲压
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 2. 计算炮口世界生成位置（相对 GunMesh 前方 456cm 处）
	const FVector MuzzleLoc = GunMesh ? GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f)) : GetActorLocation();
	const FRotator MuzzleRot = GunMesh ? GunMesh->GetComponentRotation() : GetActorRotation();

	// 3. 生成物理炮弹
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ATankProjectile::StaticClass();
	World->SpawnActor<ATankProjectile>(ClassToSpawn, MuzzleLoc, MuzzleRot, SpawnParams);

	// 4. 炮口瞬时开火闪光/示踪线
	DrawDebugLine(World, MuzzleLoc, MuzzleLoc + MuzzleRot.Vector() * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);

	UE_LOG(LogTank, Log, TEXT("[Tank] FIRE! MuzzleLoc=(%.1f, %.1f, %.1f), Reloading %.1fs"),
		MuzzleLoc.X, MuzzleLoc.Y, MuzzleLoc.Z, FireCooldown);
}

void ATankPawn::TurretRotateInput(float Value)
{
	CurrentTurretRotateInput = Value;
}

void ATankPawn::PitchUpInput(float Value)
{
	CurrentPitchInput = Value;
}
