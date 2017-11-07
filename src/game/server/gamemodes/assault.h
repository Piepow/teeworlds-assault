/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_ASSAULT_H
#define GAME_SERVER_GAMEMODES_ASSAULT_H
#include <game/server/gamecontroller.h>
#include <game/server/entity.h>
#include <game/server/broadcaster.h>

class CGameControllerAssault : public IGameController
{
public:
	// placed at the position of the red flag entity
	// no players can pick up or interact with the BaseFlag
	// If a player on the AssaultTeam holding the Assault Flag touches the BaseFlag,
	// the round is over
	class CFlag *m_pBaseFlag;
	// placed at the position of the blue flag entity
	// Only the assault team can pick up this flag
	class CFlag *m_pAssaultFlag;
	vec2 m_aFlagPositions[2];

	CGameControllerAssault(class CGameContext *pGameServer);
	virtual void DoWincheck();
	void DoBroadcasts();
	virtual bool CanBeMovedOnBalance(int ClientID);
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	virtual bool CanSpawn(int Team, vec2 *pPos);
	// virtual float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos);

	void Reset();

	virtual bool OnEntity(int Index, vec2 Pos);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
private:
	// the team that tries to capture the flag
	int m_AssaultTeam;
	// game within a game
	int m_AssaultOverTick;
	int m_AssaultStartTick;
	// we use this variable because m_AssaultStartTick has to be manipulated so that
	// the correct value appears in the client's timer. But it doesn't accurately
	// reflect the exact tick that round starts. So we use this instead. This is
	// used in calculating the flag capture time.
	int m_AssaultAbsoluteStartTick;

	// I am calling StartAssault() once at the first tick
	bool m_AssaultInitialized;

	// times required to capture the flag for both teams
	// team with lowest capture time wins
	// if it's -1, it means that team hasn't had an assault round or currently is in one,
	// and hasn't touched the flag yet
	// if it's -2, then it means that teams has already had an assault round and wasn't
	// able to touch the flag before time was up
	float m_aCaptureTime[2];

	// then it's gameover
	bool m_FinishedAllAssault;

	// give defense time to prepare
	int m_AssaultTeamSpawnDelay;

	CBroadcaster m_Broadcast;

	virtual void PostReset();

	virtual void StartRound();
	virtual void EndRound();

	void StartAssault(bool ResetWorld = true);
	void EndAssault(bool CapturedFlag = false);

	void SetAssaultFlags();

};

#endif

