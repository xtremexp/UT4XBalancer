# UT4X-TeamBalancer
UT4 plugin mutator for balancing teams at start.



## Information

### Description

UT4X Team balancer mutator is a server side mutator (or plugin) that will automatically balance teams by average ELO

when game starts.

Since it's a server side mutator, player does not need to download the mutator and no redirect is needed.

### Author

Thomas 'WinterIsComing' P.

### Version History

- 1.2 - 29/05/2021 - First public version

### License

See licence file

## Installation

- Clone repository or download repository from https://github.com/xtremexp/UT4XBalancer

- Move UT4XBalancer folder <Windows/Linux>Server/UnrealTournament/Plugins

- You should then have <Windows/Linux>Server/UnrealTournament/Plugins/UT4XBalancer folder

  

## Enable Mutator

### For hubs

#### Globally

This will allow to run for any game either from a ruleset or a custom game

- Edit <Windows/Linux>Serverr\UnrealTournament\Saved\Config\Game.ini file
- Add or edit this section

```ini
[/Script/UnrealTournament.UTGameMode]
ConfigMutators[0]=UT4XBalancer
```

#### For a specific ruleset

In your ruleset, in GameOptions, add this:

`?mutator=UT4XKickIdlers`

E.G: "gameOptions": "?mutator=UT4XBalancer"

### For dedicated server

Add this in command line:

`?mutator=UT4XBalancer`

## Configuration

Default configuration of this mutator can be changed modifying in <Windows/Linux>Server\UnrealTournament\Saved\Config\<Windows/Linux>Server\UT4X.ini file. (this file is automatically created at first start).

If file is not present you can create it with this content (see /Config/DefaultUT4X.ini file in this plugin folder) :

```ini
[/Script/UT4XBalancer.UT4XBalancer]
TeamBalancerEnabled=True
BalanceTeamsInPrivateGamesEnabled=False
```

| Property                          | Default Value | Description                                            |
| --------------------------------- | ------------- | ------------------------------------------------------ |
| TeamBalancerEnabled               | True          | Activate team balancer                                 |
| BalanceTeamsInPrivateGamesEnabled | False         | If true private games will have team balancer enabled. |

These values can be modified modifying UT4X.ini file as seen before.

## Commands

As a logged admin (command: "rconauth mypassword"), you can enable or disable mutator:

Enable TeamBalancer:

`mutate enableteambalancer`

Disable KickIdlers:

`mutate disableteambalancer`

Force team balance and randomization:

`mutate balanceteams`

Display current elo for both teams  (as a normal user)

`mutate teaminfo`



## Build UT4XBalancer

### Requirements

- Windows OS
- Microsoft Visual Studio 2015 (Community Edition (free) or paid one)
- Git client (either Git for windows or TortoiseGit)

### Procedure

- clone Unreal Tournament repository from https://github.com/EpicGames/UnrealTournament
- run "Setup.bat" in root UnrealTournament folder (might take a long time since it downloads at UT4 content (textures, maps, ...))
- clone UT4XBalancer`repository to UnrealTournament\UnrealTournament\Plugins folder
- run "GenerateProjectFiles.bat -2015" in root UnrealTournament folder
- open "UE4.sln" solution file within Visual Studio 2015
- Build plugin (first build can quite a long time since it's building UT4 as well)
  - For Windows select Shipping Server / Win64 / UE4 configuration then "Build -> Build Unreal Tournament" in menu
  - For Linux select Shipping Server / Linux / UE4 configuration then "Build -> Build Unreal Tournament" in menu
- Compiled plugin binaries will then be available within UnrealTournament\UnrealTournament\Plugins\UT4XBalancer`\Binaries folder

### Notes

UT4X Balancer has some experimental features (disabled in code) not yet properly working such as:

- picking up the right team for new player joining game in progress
- auto-switch player if teams become uneven after some player left

You may try to fix them.