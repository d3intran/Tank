#include "TankPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根碰撞盒：根据缩放 0.08 后的真实尺寸（长7~8米，宽3.5米，高2.4米）
	// HalfExtent: X=380cm (长7.6m), Y=175cm (宽3.5m), Z=120cm (高2.4m)
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(380.0f, 175.0f, 120.0f));
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetSimulatePhysics(false);
	RootComponent = CollisionBox;

	// 2. 车身 Mesh（按 0.08 缩放，使其严丝合缝匹配现实与沙盘网格）
	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(CollisionBox);
	HullMesh->SetRelativeLocation(FVector(-130.0f, 0.0f, -120.0f));
	HullMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.08f));
	HullMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMeshAsset(TEXT("/Game/tank/ztz-88a/mesh_317.mesh_317"));
	if (HullMeshAsset.Succeeded())
	{
		HullMesh->SetStaticMesh(HullMeshAsset.Object);
	}

	// 3. 履带 Mesh
	TracksMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracksMesh"));
	TracksMesh->SetupAttachment(HullMesh);
	TracksMesh->SetRelativeLocation(FVector::ZeroVector);
	TracksMesh->SetRelativeScale3D(FVector::OneVector);
	TracksMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TracksMeshAsset(TEXT("/Game/tank/ztz-88a/mesh_318.mesh_318"));
	if (TracksMeshAsset.Succeeded())
	{
		TracksMesh->SetStaticMesh(TracksMeshAsset.Object);
	}

	// 4. 炮管俯仰轴心
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(HullMesh);
	GunPivot->SetRelativeLocation(FVector(100.0f, 0.0f, 1500.0f));

	// 5. 第三人称相机弹簧臂（向后拉出 9 米，向上抬高 2.5 米，俯视 12 度对准车身）
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = 900.0f;
	SpringArm->SetRelativeLocation(FVector(-100.0f, 0.0f, 220.0f));
	SpringArm->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;

	// 6. 相机
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();

	if (TracksMesh && TracksMesh->GetNumMaterials() > 0)
	{
		TrackMaterialInstance = TracksMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. WASD 前后移动
	if (!FMath::IsNearlyZero(CurrentMoveInput))
	{
		FVector MoveDelta = FVector(CurrentMoveInput * MoveSpeed * DeltaTime, 0.0f, 0.0f);
		FHitResult Hit;
		AddActorLocalOffset(MoveDelta, true, &Hit);

		if (TrackMaterialInstance)
		{
			TrackUVOffset += CurrentMoveInput * (MoveSpeed / 300.0f) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 2. WASD 原地差速掉头
	if (!FMath::IsNearlyZero(CurrentTurnInput))
	{
		FRotator TurnDelta = FRotator(0.0f, CurrentTurnInput * TurnSpeed * DeltaTime, 0.0f);
		AddActorLocalRotation(TurnDelta, true);

		if (TrackMaterialInstance && FMath::IsNearlyZero(CurrentMoveInput))
		{
			TrackUVOffset += CurrentTurnInput * (TurnSpeed / 30.0f) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 3. Q / E 控制炮台 360 度水平旋转 (Yaw)
	if (!FMath::IsNearlyZero(CurrentTurretRotateInput))
	{
		CurrentTurretYaw = FRotator::NormalizeAxis(CurrentTurretYaw + CurrentTurretRotateInput * TurretRotateSpeed * DeltaTime);
	}

	// 4. 鼠标上下移动微调炮管仰角 (Pitch) - 限制范围 [MinPitch, MaxPitch]
	if (!FMath::IsNearlyZero(CurrentPitchInput))
	{
		CurrentPitch = FMath::Clamp(CurrentPitch + CurrentPitchInput * PitchSpeed * DeltaTime, MinPitch, MaxPitch);
	}

	// 更新炮台旋转（水平 Yaw 360° + 垂直 Pitch 微调）
	if (GunPivot)
	{
		GunPivot->SetRelativeRotation(FRotator(CurrentPitch, CurrentTurretYaw, 0.0f));
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

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ATankPawn::MoveForwardInput);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ATankPawn::TurnInput);
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

void ATankPawn::TurretRotateInput(float Value)
{
	CurrentTurretRotateInput = Value;
}

void ATankPawn::PitchUpInput(float Value)
{
	CurrentPitchInput = Value;
}
