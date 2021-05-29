#pragma once

#include "Core.h"
#include "UnrealTournament.h"
#include "UTMutator.h"

#include "UT4XBalancer.generated.h"


enum EBalanceType
{
	BT_LAST_TO_JOIN,
	BT_LAST_TO_JOIN_ALIVE,
	BT_ELO_BEST_MATCH,
	BT_BBB
};

UCLASS(Config= UT4X, Blueprintable, Meta = (ChildCanTick))
class AUT4XBalancer : public AUTMutator
{

	GENERATED_UCLASS_BODY()

	/*
	* If true team balancer is enabled
	*/
	UPROPERTY(Config = UT4X)
	bool TeamBalancerEnabled = true;

	/*
	* Whether team balancer is enabled for private games (passworded, lan, ...)
	*/
	UPROPERTY(Config = UT4X)
	bool BalanceTeamsInPrivateGamesEnabled = false;

	void Start(UWorld* World);
	void Stop();


	// UT Mutator implemantations
	void Init_Implementation(const FString& Options) override;

	/* Some user and admin commands */
	void Mutate_Implementation(const FString& MutateString, APlayerController* Sender) override;

	/* For picking up best team for new player - Experimental - Disabled */
	void PostPlayerInit_Implementation(AController* C) override;

	/* For checking teams are still even after some player left - Experimental - Disabled */
	void NotifyLogout_Implementation(AController* C) override;

	/* For balancing teams at game start */
	void NotifyMatchStateChange_Implementation(FName NewState) override;


private:

	/*
	* Balance teams at start, must not be used during game else will randomize  all players !
	*/ 
	bool BalanceTeamsAtStart();

	/*
	* Get total elo for the specified members (team)
	*/
	int32 TotalEloForTeamMembers(AUTGameMode* GameMode, TArray<AController*> TeamMembers);

	/*
	* Get elo from player
	*/
	int32 GetEloForPlayer(AUTGameMode* GameMode, AController* C);

	/*
	* Switches player to specified new team
	*/
	bool SwitchPlayerToTeam(AController* C, int8 NewTeamIdx, AUTTeamGameMode* TeamGM, AUTGameState* UTGameState);

	/*
	* Executed by timer after 20s when a player logout
	*/
	void CheckAndBalanceTeams();

	/*
	* Global function
	*/
	bool CanBalanceTeams();

	void BroadcastMessageToPlayers(FString& Msg, FName ChatDestination);

	/*
	* Pick player from team (used for team balancing)
	*/
	AController* PickPlayerToSwapFromTeam(int32 TeamIdx, EBalanceType BalanceType);

	/*
	* Timer to check teams each 20s some player has left - Experimental - Disabled
	*/
	FTimerHandle CheckUnbalancedTeamsTimerHandle;

	/*
	* Returns winning team index
	*/
	int32 GetTeamWinningIdx();
};