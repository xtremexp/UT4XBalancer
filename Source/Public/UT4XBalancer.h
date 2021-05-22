#pragma once

#include <iostream>
#include "Core.h"
#include "UnrealTournament.h"
#include "UTMutator.h"

#include "UT4XBalancer.generated.h"


UCLASS(Config=UT4XBalancer, Blueprintable, Meta = (ChildCanTick))
class AUT4XBalancer : public AUTMutator
{

	GENERATED_UCLASS_BODY()

	/*
	* If true team balancer will be enabled
	*/
	UPROPERTY(Config = UT4XBalancer)
	bool TeamBalancerEnabled = true;

	/*
	* Time until player is warned to be AFK
	*/
	UPROPERTY(Config = UT4XBalancer)
	int AFKWarnTime = 60;

	/*
	* Time until player is kick 
	*/
	UPROPERTY(Config = UT4XBalancer)
	int AFKKickTime = 90;

	UPROPERTY(Config = UT4XBalancer)
	bool AFKCheckerEnabled = true;


	void Start(UWorld* World);
	void Stop();


	// UT Mutator implemantations
	void Init_Implementation(const FString& Options) override;

	void Mutate_Implementation(const FString& MutateString, APlayerController* Sender) override;

	void PostPlayerInit_Implementation(AController* C) override;

	/* For log player exit info */
	void NotifyLogout_Implementation(AController* C) override;

	void ModifyLogin_Implementation(UPARAM(ref) FString& Portal, UPARAM(ref) FString& Options) override;

	void NotifyMatchStateChange_Implementation(FName NewState) override;


private:

	/*
	* Balance teams at start, must not be used during game else will randomize  all players !
	*/ 
	bool BalanceTeamsAtStart();

	int32 TotalEloForTeamMembers(AUTGameMode* GameMode, TArray<AController*> TeamMembers);

	int32 GetEloForPlayer(AUTGameMode* GameMode, AController* C);

	bool SwitchPlayerToTeam(AController* C, int8 NewTeamIdx, AUTTeamGameMode* TeamGM, AUTGameState* UTGameState);

	/*
	* Check players idling and if so kick them 
	* depending 
	*/
	void CheckPlayersIdling();

	/*
	* Executed by timer after 20s when a player logout
	*/
	void CheckAndBalanceTeams();

	bool CanBalanceTeams();

	void BroadcastMessageToPlayers(FString& Msg, FName ChatDestination);

	/*
	* Pick player from team (used for team balancing)
	*/
	AController* PickPlayerToSwapFromTeam(int32 TeamIdx, EBalanceType BalanceType);

	FTimerHandle CheckPlayerIdlingTimerHandle;

	FTimerHandle CheckUnbalancedTeamsTimerHandle;

	int32 GetTeamWinningIdx();

	void LogTeamInfo();
};