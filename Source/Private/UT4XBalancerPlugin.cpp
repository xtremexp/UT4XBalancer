
#include "UT4XBalancer.h"

#include "ModuleManager.h"
#include "ModuleInterface.h"

class FUT4XBalancerPlugin : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);

	AUT4XBalancer* _UT4XBalancer;
};

IMPLEMENT_MODULE( FUT4XBalancerPlugin, UT4XBalancer )

//================================================
// Startup
//================================================

void FUT4XBalancerPlugin::StartupModule()
{
	UE_LOG(UT, Log, TEXT("StartupModule UT4X Balancer"));

	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FUT4XBalancerPlugin::OnWorldInitialized);
}

//================================================
// Shutdown
//================================================

void FUT4XBalancerPlugin::ShutdownModule()
{
	UE_LOG(UT, Log, TEXT("ShutdownModule UT4X UT4XBalancer"));

	if (_UT4XBalancer) {
		_UT4XBalancer->Stop();
	}
}

void FUT4XBalancerPlugin::OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (World)
	{

		if (_UT4XBalancer == NULL) {
			_UT4XBalancer = World->SpawnActor<AUT4XBalancer>();

			_UT4XBalancer->Start(World);
		}
	}
}
