#include "UT4XBalancer.h"
#include "UTFlagRunGame.h"

// folder name of this module in plugins folder
#define DEBUGLOG Verbose

AUT4XBalancer::AUT4XBalancer(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{

	DisplayName = FText::FromString("UT4X Balancer 1.2.0");
	Author = FText::FromString("Thomas 'XtremeXp/Winter' P.");
	Description = FText::FromString("Team balancer module for Unreal Tournament 4 (2017) ");
}

// this function is called from module when world is loaded
void AUT4XBalancer::Start(UWorld* World)
{
	// Don't garbage collect me
	SetFlags(RF_MarkAsRootSet);
}

void AUT4XBalancer::Stop()
{
	// NOTHING YET
}

void AUT4XBalancer::Init_Implementation(const FString& Options)
{
	UE_LOG(UT, Log, TEXT("UT4X team balancer mutator init %s "), *Options);

	TeamBalancerEnabled = (UGameplayStatics::GetIntOption(Options, TEXT("TeamBalancerEnabled"), TeamBalancerEnabled) == 0) ? false : true;
	
	Super::Init_Implementation(Options);
}

void AUT4XBalancer::Mutate_Implementation(const FString& MutateString, APlayerController* Sender)
{

	TArray<FString> Tokens;
	AUTPlayerController* UTSender = Cast<AUTPlayerController>(Sender);

	if (UTSender) {
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(Sender->PlayerState);

		FString MutateStringTrimmed = MutateString;
		// have to trim because there is a bug with mutate commande which adds extra space if > 1 parameter
		MutateStringTrimmed.Trim().ParseIntoArray(Tokens, TEXT(" "), false);
		bool isInvalid = false;


		// TODO enable only if "rconauthed" !
		if (UTPS && Tokens.Num() == 1) {
			

			if (UTPS->bIsRconAdmin) {

				if (Tokens[0] == "balanceteams") {
					if (!BalanceTeamsAtStart()) {
						UTSender->ClientSay(UTPS, TEXT("Could not balance teams (not a team game mode, disabled or not enough players)."), ChatDestinations::System);
					}
				}
				else if (Tokens[0] == "enableteambalancer") {
					TeamBalancerEnabled = true;
					SaveConfig();
					UTSender->ClientSay(UTPS, TEXT("Team balancer has been enabled."), ChatDestinations::System);
				}
				else if (Tokens[0] == "disableteambalancer") {
					TeamBalancerEnabled = false;
					SaveConfig();
					UTSender->ClientSay(UTPS, TEXT("Team balancer has been disabled."), ChatDestinations::System);
				}
			}
			
			// display for player current total elo for each teams
			// "mutate teaminfo"
			if (Tokens[0] == "teaminfo") {

				AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
				AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

				if (UTGameState && TeamGM) {
					TArray<AUTTeamInfo*> Teams = UTGameState->Teams;

					int totalEloRed = TotalEloForTeamMembers(TeamGM, Teams[0]->GetTeamMembers());
					int totalEloBlue = TotalEloForTeamMembers(TeamGM, Teams[1]->GetTeamMembers());

					UTSender->ClientSay(UTPS, FString::Printf(TEXT("Elo Red: %i - Elo: %i"), totalEloRed, totalEloBlue), ChatDestinations::System);
				}
			}
		}
	}

	Super::Mutate_Implementation(MutateString, Sender);
}


/*
* After player has joined the game
*/
void AUT4XBalancer::PostPlayerInit_Implementation(AController* C) {

	AUTPlayerController* PC = Cast<AUTPlayerController>(C);

	if (PC && PC->PlayerState && !PC->PlayerState->bIsABot) {
		
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(PC->PlayerState);
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

		// Team game try to pickup best team for player only when game has started
		// have to check gamestate nullsafe since HasMatchStarted is not null safe ...
		if (TeamGM && TeamGM->GameState && CanBalanceTeams() && TeamGM->HasMatchStarted()) {
			AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();

			// FIXME does not switch to good team for some unknown reason
			if (false && UTGameState) {
				TArray<AUTTeamInfo*> Teams = UTGameState->Teams;
				int8 NewTeamIdx = -1;
				
				// team size not same
				if (Teams[0]->GetSize() > Teams[1]->GetSize()) {
					NewTeamIdx = 1;
				} else if (Teams[0]->GetSize() < Teams[1]->GetSize()) {
					NewTeamIdx = 0;
				}
				// same team sizes - pick team with lowest average elo
				// TODO pick losing team maybe instead
				else {
					int32 averageEloRed = Teams[0]->AverageEloFor(TeamGM);
					int32 averageEloBlue = Teams[1]->AverageEloFor(TeamGM);

					// red has better elo
					if (averageEloRed > averageEloBlue) {
						NewTeamIdx = 1; // pick blue team
					} 
					// blue has better elo
					else if (averageEloRed < averageEloBlue) {
						NewTeamIdx = 0; // pick red team
					}
				}

				if (NewTeamIdx != -1 && TestFeatureEnabled) {
					SwitchPlayerToTeam(PC, NewTeamIdx, TeamGM, UTGameState);
				}
			}
		}

	}

	Super::PostPlayerInit_Implementation(C);
}


void AUT4XBalancer::NotifyLogout_Implementation(AController* C) {

	APlayerController* PC = Cast<APlayerController>(C);

	if (PC && PC->PlayerState && !PC->PlayerState->bIsABot) {

		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

		// once player leave check and fix if teams are unbalanced
		if (TeamBalancerEnabled && TeamGM && TeamGM->GameState && TeamGM->HasMatchStarted() && !TeamGM->HasMatchEnded()) {

			if (GetWorldTimerManager().IsTimerActive(CheckUnbalancedTeamsTimerHandle)) {
				GetWorldTimerManager().ClearTimer(CheckUnbalancedTeamsTimerHandle);
			}

			GetWorldTimerManager().SetTimer(CheckUnbalancedTeamsTimerHandle, this, &AUT4XBalancer::CheckAndBalanceTeams, 20.f, false, 15.f);
		}
	}

	Super::NotifyLogout_Implementation(C);
}



/*
* Match states changed from blitz
PlayerIntro
MatchIntermission
InProgress
InProgress
MatchIntermission
InProgress
InProgress
...
WaitingPostMatch
MapVoteHappening
*/
void AUT4XBalancer::NotifyMatchStateChange_Implementation(FName NewState)
{

	AUTFlagRunGame* FlagRunGM = Cast<AUTFlagRunGame>(GetWorld()->GetAuthGameMode());

	// balance teams for all gamemode but blitz (because never reaches this matchstate / prob a vanilla bug)
	if (NewState == MatchState::CountdownToBegin && FlagRunGM == NULL && TeamBalancerEnabled) {
		BalanceTeamsAtStart();
	}
	// blitz gamemode never triggers CountdownToBegin matchstate so have to trigger at this next match state
	else if (NewState == MatchState::PlayerIntro && FlagRunGM && TeamBalancerEnabled) {
		BalanceTeamsAtStart();
	}
}

// TODO handle teams with bots
// TODO handle teams count > 2
bool AUT4XBalancer::BalanceTeamsAtStart() {

	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

	if (UTGameState && CanBalanceTeams()) {

		TArray<AUTTeamInfo*> Teams = UTGameState->Teams;

		TArray<AController*> SortedPlayersByElo;

		for (int32 i = 0; i < Teams.Num(); i++)
		{
			for (AController* C : Teams[i]->GetTeamMembers()) {
				SortedPlayersByElo.Add(C);
			}
		}

		// not enough players no need to balance ...
		if (SortedPlayersByElo.Num() <= 2) {
			return false;
		}

		// 1 - make a list with all players sorted by elo
		SortedPlayersByElo.Sort([&](AController& A, AController& B) {

			AUTPlayerState* TeamPSA = Cast<AUTPlayerState>(A.PlayerState);
			int32 eloA = NEW_USER_ELO;

			if (TeamPSA)
			{
				eloA = GetEloForPlayer(TeamGM, &A);
			}

			AUTPlayerState* TeamPSB = Cast<AUTPlayerState>(B.PlayerState);
			int32 eloB = NEW_USER_ELO;

			if (TeamPSB)
			{
				eloB = GetEloForPlayer(TeamGM, &B);
			}

			return eloA > eloB;
		});


		int32 NewTeamIdx = 0;

		// 2 - switch alternatively to blue or red team for sorted elo list
		for (AController* C : SortedPlayersByElo) {

			SwitchPlayerToTeam(C, NewTeamIdx, TeamGM, UTGameState);
			
			NewTeamIdx++;

			if (NewTeamIdx >= UTGameState->Teams.Num()) {
				NewTeamIdx = 0;
			}
		}

		
		int32 averageEloRed = Teams[0]->AverageEloFor(TeamGM);
		int32 averageEloBlue = Teams[1]->AverageEloFor(TeamGM);

		int32 totalEloRed = TotalEloForTeamMembers(TeamGM, Teams[0]->GetTeamMembers());
		int32 totalEloBlue = TotalEloForTeamMembers(TeamGM, Teams[1]->GetTeamMembers());
		int32 eloDiffAbs = FMath::Abs(totalEloRed - totalEloBlue);

		bool recomputeEloStats = false;

		// because the upper algorythm make best player in red team
		// and worst in blue team (if same player count on both teams)
		// red team is nearly always more powered so we need to switch one extra guy to balance
		if (SortedPlayersByElo.Num() >= 6) {
			// if red total red elo is 11000 and total blue elo is 10000
			// there is still a 10% gap which might be critical !

			// need to switch a guy from red team with elo close to : AvgElo[Red] + (11000 -10000) = AveEloRed + 500
			// need switch a guy from blue with closest elo from: AvgElo[Blue] - 500 = AvgEloRed - 500

			int32 totalEloTeams = totalEloRed + totalEloBlue;
			int32 totalTeamsSize = Teams[0]->GetSize() + Teams[1]->GetSize();

			// EG: Red Team with totalElo = 11000 with 5 players - avg elo = 11000 / 5 = 2200
			// EG: Blue Team with TotalElo = 12000 with 6 players  - avg elo = 12000 / 6 = 2000
			// avg elo = 23000 / 11 = 2091
			int32 avgEloTeams = (totalEloTeams) / totalTeamsSize;

			// EG: desiredEloRed = 5 * 2091 = 10455 (current: 11000)
			int32 desiredTeamEloRed = Teams[0]->GetSize() * avgEloTeams;
			
			// EG: desiredEloBlue = 6 * 2091 = 12546 (current: 12000)
			int32 desiredTeamEloBlue = Teams[1]->GetSize() * avgEloTeams;

			// we should switch then a red player with elo:
			// 2091 + (11000 - 10455)/2 = 2091 + (11000 - 10455)/2 = 2091 + 272 = 2363
			int32 desiredEloRedToSwitch = avgEloTeams + (totalEloRed - desiredTeamEloRed)/2;

			// we should switch then a blue player with elo:
			// 2091 + (12000 - 12546)/2 = 2091 - (12000 - 12546)/2 = 2091 - 272 = 1819
			int32 desiredEloBlueToSwitch = avgEloTeams + (totalEloBlue - desiredTeamEloBlue)/2;

			AController* CRedSwitch = Teams[0]->MemberClosestToElo(TeamGM, desiredEloRedToSwitch);
			AController* CBlueSwitch = Teams[1]->MemberClosestToElo(TeamGM, desiredEloBlueToSwitch);

			// so final elo for red should be: 11000 - 2363 + 1819 = 10456 -> 12091 / 5 ~=  2090 (average elo)
			// so final elo for blue should be: 12000 - 1819 + 2363 = 12544 -> 10909 / 6 ~= 2090 (average elo)
			if (CRedSwitch && CBlueSwitch) {

				int32 eloSwitchDiff = GetEloForPlayer(TeamGM, CRedSwitch) - GetEloForPlayer(TeamGM, CBlueSwitch);
				int32 newTotalEloRed = totalEloRed - eloSwitchDiff;
				int32 newTotalEloBlue = totalEloBlue + eloSwitchDiff;
				int32 newEloDiffAbs = FMath::Abs(newTotalEloRed - newTotalEloBlue);


				// elo difference between teams is smaller so it's better so we can apply the swaps ...
				if (newEloDiffAbs < eloDiffAbs) {
					SwitchPlayerToTeam(CRedSwitch, 1, TeamGM, UTGameState);
					SwitchPlayerToTeam(CBlueSwitch, 0, TeamGM, UTGameState);

					int32 newAverageEloRed = Teams[0]->AverageEloFor(TeamGM);
					int32 newAverageEloBlue = Teams[1]->AverageEloFor(TeamGM);
					recomputeEloStats = true;
				}
			}
		}

		// compute again total elo for both team if some extra player has been switched
		if (recomputeEloStats) {
			totalEloRed = TotalEloForTeamMembers(TeamGM, Teams[0]->GetTeamMembers());
			totalEloBlue = TotalEloForTeamMembers(TeamGM, Teams[1]->GetTeamMembers());

			averageEloRed = Teams[0]->AverageEloFor(TeamGM);
			averageEloBlue = Teams[1]->AverageEloFor(TeamGM);
		}


		FString Msg1 = FString::Printf(TEXT("[UT4X Balancer v1.2] - Tot Elo: Red %i - Blue %i - (%ip vs %ip)"), totalEloRed, totalEloBlue, Teams[0]->GetSize(), Teams[1]->GetSize());
		BroadcastMessageToPlayers(Msg1, ChatDestinations::System);

		FString Msg2 = FString::Printf(TEXT("[UT4X Balancer v1.2] - Avg Elo: Red %i - Blue %i"), averageEloRed, averageEloBlue);
		BroadcastMessageToPlayers(Msg2, ChatDestinations::System);


		return true;
	}

	return false;
}

// only allow usage of balancer if
// team game
// more than 4 players
// not a vsAi Game
// not a passworded game
// 2 teams (not handling more than 2)
bool AUT4XBalancer::CanBalanceTeams() {

	AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

	if (TeamGM) {
		return TeamBalancerEnabled 
			&& (TeamGM->NumPlayers + TeamGM->NumBots) >= 4
			&& !TeamGM->bIsVSAI 
			&& TeamGM->bBalanceTeams
			&& !TeamGM->bRequirePassword 
			&& TeamGM->Teams.Num() == 2;
	}

	return false;
}


void AUT4XBalancer::CheckAndBalanceTeams() {

	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

	// FIXME for some unknown reason does not switch good player to good team
	if (false && UTGameState && TeamGM && CanBalanceTeams()) {

		// TODO change use other BalanceType depending on team scores, match started or not ...
		// TODO use BT_LAST_TO_JOIN if team with least players winning
		EBalanceType BalanceType = BT_LAST_TO_JOIN_ALIVE;
		AController* PlayerToSwap = nullptr;
		int NewTeamSwapIdx = -1;

		// more players in red team need to switch one guy from this team
		if (UTGameState->Teams[0]->GetSize() - UTGameState->Teams[1]->GetSize() >= 2) {
			NewTeamSwapIdx = 1;
			PlayerToSwap = PickPlayerToSwapFromTeam(0, BalanceType);
		}
		// more players in blue team
		else if (UTGameState->Teams[1]->GetSize() - UTGameState->Teams[0]->GetSize() >= 2) {
			NewTeamSwapIdx = 0;
			PlayerToSwap = PickPlayerToSwapFromTeam(1, BalanceType);
		}

		if (PlayerToSwap && PlayerToSwap->PlayerState && NewTeamSwapIdx > -1) {

			UE_LOG(UT, Log, TEXT("Balancer wants to switch %s to team %i to balance teams"), *PlayerToSwap->PlayerState->PlayerName, NewTeamSwapIdx);

			if (SwitchPlayerToTeam(PlayerToSwap, NewTeamSwapIdx, TeamGM, UTGameState)) {
				FString Msg = FString::Printf(TEXT("[UT4X Balancer v1.2] %s has been swapped to balance teams. (last alive player to join)"), *PlayerToSwap->PlayerState->PlayerName, AFKKickTime);
				BroadcastMessageToPlayers(Msg, ChatDestinations::System);
			}
		}
	}
}

AController* AUT4XBalancer::PickPlayerToSwapFromTeam(int32 TeamIdx, EBalanceType BalanceType) {

	AController* PlayerToSwap = nullptr;

	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());

	if (TeamGM && UTGameState && UTGameState->Teams.IsValidIndex(TeamIdx)) {

		int32 MaxStartTime = 0;
		AUTTeamInfo* team = UTGameState->Teams[TeamIdx];

		// pickup player from team alive who joined last
		if (BalanceType == BT_LAST_TO_JOIN || BalanceType == BT_LAST_TO_JOIN_ALIVE) {
			for (AController* C : team->GetTeamMembers()) {

				AUTPlayerState* UTPS = Cast<AUTPlayerState>(C->PlayerState);

				// do not switch member out of lives (e.g: blitz) or carrying object (flag)
				if (UTPS && !UTPS->bOutOfLives && UTPS->CarriedObject == nullptr) {

					// pick the player who joined lately
					// starttime = how long playerstate is initialized
					if (UTPS->StartTime > MaxStartTime) {
						MaxStartTime = UTPS->StartTime;
						PlayerToSwap = C;
					}
				}
			}
		}
		// pick up player so the difference with other team is even
		// this might be
		else if (BalanceType == BT_ELO_BEST_MATCH) {

			int32 OtherTeamIdx = -1;

			// E.G: 
			// Red Team: 6p with 10000 elo
			// BLue Team: 4p with 7000 elo
			// we need to pick up a player from red team (Idx: 0) that will be switch to blue
			if (TeamIdx == 0) {
				OtherTeamIdx = 1;
			}
			else if (TeamIdx == 1) {
				OtherTeamIdx = 0;
			}

			int32 otherTeamElo = TotalEloForTeamMembers(TeamGM, UTGameState->Teams[OtherTeamIdx]->GetTeamMembers());

			// need to pickup a player from this team so the team has nearly save average elo as the other one
			int32 teamElo = TotalEloForTeamMembers(TeamGM, UTGameState->Teams[TeamIdx]->GetTeamMembers());

			// 17000 / 10 = 1700
			int32 avgEloTeams = (otherTeamElo + teamElo) / (UTGameState->Teams[1]->GetSize() + UTGameState->Teams[0]->GetSize());

			// desired new team elo since we gonna swap one player from this team
			// For red: 1700 * 5 = 8500
			int32 newDesiredTeamElo = avgEloTeams * (UTGameState->Teams[TeamIdx]->GetSize() - 1);

			// 10000 - 8500 = 1500
			int32 DesiredPlayerEloSwap = teamElo - newDesiredTeamElo;
			PlayerToSwap = UTGameState->Teams[TeamIdx]->MemberClosestToElo(TeamGM, DesiredPlayerEloSwap);
		}
	}

	return PlayerToSwap;
}

int32 AUT4XBalancer::TotalEloForTeamMembers(AUTGameMode* GameMode, TArray<AController*> TeamMembers) {

	if (GameMode)
	{
		int32 TeamElo = 0;

		for (AController* C : TeamMembers)
		{
			int32 NewElo = GetEloForPlayer(GameMode, C);
			TeamElo += NewElo;
		}
		return TeamElo;
	}

	return 0;
}

// duplicated code fro UTTeamInfo.cpp
int32 AUT4XBalancer::GetEloForPlayer(AUTGameMode* GameMode, AController* C) {

	if (GameMode && C) {
		AUTPlayerState* TeamPS = Cast<AUTPlayerState>(C->PlayerState);
		if (TeamPS) {
			return GameMode->IsValidElo(TeamPS, GameMode->bRankedSession) ? GameMode->GetEloFor(TeamPS, GameMode->bRankedSession) : NEW_USER_ELO;
		}
	}

	return 0;
}

// adapted code from UTTeamGameMode.cpp
bool AUT4XBalancer::SwitchPlayerToTeam(AController* C, int8 NewTeamIdx, AUTTeamGameMode* TeamGM, AUTGameState* UTGameState) {

	if (TeamGM && UTGameState && C) {
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(C->PlayerState);

		if (UTPS) {
			if (UTGameState->Teams.IsValidIndex(NewTeamIdx) && (UTPS->Team == NULL || UTPS->Team->TeamIndex != NewTeamIdx))
			{
				AUTCharacter* UTC = Cast<AUTCharacter>(C->GetPawn());
				if (UTC != nullptr)
				{
					UTC->PlayerSuicide();
				}

				if (UTPS->Team != NULL)
				{
					UTPS->Team->RemoveFromTeam(C);
				}
				UTGameState->Teams[NewTeamIdx]->AddToTeam(C);
				UTPS->bPendingTeamSwitch = false;

				if (TeamGM->GetMatchState() != MatchState::WaitingPostMatch)
				{
					AUTPlayerController* UTPC = Cast<AUTPlayerController>(C);
					static const FName VoiceChatTokenFeatureName("VoiceChatToken");
					if (UTPC && UTPS && UTPS->Team && !UTPS->bOnlySpectator && IModularFeatures::Get().IsModularFeatureAvailable(VoiceChatTokenFeatureName))
					{
						UTVoiceChatTokenFeature* VoiceChatToken = &IModularFeatures::Get().GetModularFeature<UTVoiceChatTokenFeature>(VoiceChatTokenFeatureName);
						UTPC->VoiceChatChannel = UTPS->Team->VoiceChatChannel;
						VoiceChatToken->GenerateClientJoinToken(UTPC->VoiceChatPlayerName, UTPC->VoiceChatChannel, UTPC->VoiceChatJoinToken);

#if WITH_EDITOR
						if (UTPC->IsLocalPlayerController())
						{
							UTPC->OnRepVoiceChatJoinToken();
						}
#endif
					}
				}

				UTPS->ForceNetUpdate();

				// TODO ADD MISSING CODE FROM UTTEAMGAMEMODE

				UTPS->HandleTeamChanged(C);

				return true;
			}
		}
	}

	return false;
}

void AUT4XBalancer::BroadcastMessageToPlayers(FString& Msg, FName ChatDestination) {

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* NextPlayer = Cast<AUTPlayerController>(*Iterator);

		if (NextPlayer && NextPlayer->UTPlayerState && !NextPlayer->PlayerState->bIsABot)
		{
			NextPlayer->ClientSay(nullptr, Msg, ChatDestination);
		}
	}
}


int32 AUT4XBalancer::GetTeamWinningIdx() {

	AUTTeamGameMode* TGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();

	if (TGM && UTGameState) {

		AUTFlagRunGame* FlagRunGM = Cast<AUTFlagRunGame>(TGM);

		// in blitz there are several rounds
		if (FlagRunGM) {
			// TODO TODO 
		}
		else {
			if (UTGameState->Teams[0]->Score > UTGameState->Teams[1]->Score) {
				return 0;
			}
		}
	}

	return -1;
}
