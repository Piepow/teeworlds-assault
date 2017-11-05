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
	class CFlag *m_pFlag;
	vec2 m_FlagPosition;

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

