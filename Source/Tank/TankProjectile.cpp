#include "TankProjectile.h"
#include "Tank.h"
#include "Zombie.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ATankProjectile::ATankProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 碰撞体（根组件）
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(15.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// 显式对丧尸自定义通道 Block：profile 对未命名 GameTrace 通道的响应不可靠，
	// 缺这行炮弹会直穿丧尸本体（只在打地/打墙时溅射杀伤）
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->OnComponentHit.AddDynamic(this, &ATankProjectile::OnHit);
	RootComponent = CollisionSphere;

	// 2. 炮弹视觉网格体（使用引擎基础球体作为弹丸雏形）
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.6f)); // 拉伸为弹头形状
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMeshAsset.Object);
	}

	// 3. 弹道运动组件（设置现代坦克穿甲弹/高爆弹基准速度与微重力弹道）
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 18000.0f;     // 180 m/s 兼顾高速与视觉轨迹
	ProjectileMovement->MaxSpeed = 25000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.35f; // 轻微重力下坠，体现远距离弹道弧线

	InitialLifeSpan = 6.0f; // 6秒未命中自动销毁，防止内存泄漏
}

void ATankProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void ATankProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 忽略与发射者（自身坦克）的自相碰撞
	if (OtherActor && (OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator()))
	{
		return;
	}

	// 绘制命中爆炸光环与冲击点
	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, Hit.ImpactPoint, ExplosionRadius * 0.5f, 16, FColor::Orange, false, 2.0f, 0, 2.5f);
		DrawDebugPoint(World, Hit.ImpactPoint, 20.0f, FColor::Red, false, 2.0f);
	}

	// 若击中物理模拟物体，施加物理冲量
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		OtherComp->AddImpulseAtLocation(GetVelocity() * 20.0f, Hit.ImpactPoint);
	}

	// 施加点伤害
	if (OtherActor)
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, GetVelocity().GetSafeNormal(), Hit, GetInstigatorController(), this, nullptr);
	}

	// P1 溅射：落点径向杀伤尸群（半半径内全额击杀，外圈 0.3× 打残；P2 迁 Mass 后由空间索引取代遍历）
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AZombie> ZombieIt(World); ZombieIt; ++ZombieIt)
		{
			AZombie* Zombie = *ZombieIt;
			if (!Zombie || Zombie->IsDead())
			{
				continue;
			}
			const float Dist2D = FVector::Dist2D(Zombie->GetActorLocation(), Hit.ImpactPoint);
			if (Dist2D <= ExplosionRadius)
			{
				const float SplashDamage = Dist2D <= ExplosionRadius * 0.5f ? Damage : Damage * 0.3f;
				UGameplayStatics::ApplyPointDamage(Zombie, SplashDamage, GetVelocity().GetSafeNormal(), Hit, GetInstigatorController(), this, nullptr);
			}
		}
	}

	UE_LOG(LogTank, Log, TEXT("[TankProjectile] Hit: %s at %s"), 
		OtherActor ? *OtherActor->GetName() : TEXT("None"), 
		*Hit.ImpactPoint.ToString());

	Destroy();
}
