/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "assault.h"

#define READYPLAYER(C) (((C) < 0 || (C) >= MAX_CLIENTS) ? 0 : (GameServer()->IsClientReady(C) && GameServer()->IsClientPlayer(C)) ? GameServer()->m_apPlayers[C] : 0)

CGameControllerAssault::CGameControllerAssault(class CGameContext *pGameServer)
: IGameController(pGameServer), m_Broadcast(pGameServer)
{
	m_pFlag = 0;
	m_pGameType = "Assault";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	// arbitrary
	m_AssaultTeam = TEAM_BLUE;
	m_aCaptureTime[0] = -1.0f;
	m_aCaptureTime[1] = -1.0f;
	m_AssaultOverTick = -1;
	m_AssaultStartTick = Server()->Tick();
	m_FinishedAllAssault = false;
	if (g_Config.m_SvAssaultTeamSpawnDelay > 0)
	{
		m_AssaultTeamSpawnDelay = g_Config.m_SvAssaultTeamSpawnDelay * Server()->TickSpeed();
	}
	else
	{
		m_AssaultTeamSpawnDelay = -1;
	}
	m_AssaultInitialized = false;

	Reset();
}

void CGameControllerAssault::PostReset()
{
	IGameController::PostReset();
	Reset();
}

void CGameControllerAssault::Reset()
{
	m_Broadcast.Reset();
}

bool CGameControllerAssault::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if(Index == ENTITY_FLAGSTAND_RED)
	{
		m_FlagPosition = Pos;
	}

	return true;
}

int CGameControllerAssault::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, WeaponID);
	int HadFlag = 0;

	// drop flag
	CFlag *F = m_pFlag;
	if(F && pKiller && pKiller->GetCharacter() && F->m_pCarryingCharacter == pKiller->GetCharacter())
		HadFlag |= 2;
	if(F && F->m_pCarryingCharacter == pVictim)
	{
		GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
		F->m_DropTick = Server()->Tick();
		F->m_pCarryingCharacter = 0;
		F->m_Vel = vec2(0,0);

		if(pKiller && pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam())
			pKiller->m_Score++;

		HadFlag |= 1;
	}

	return HadFlag;
}

void CGameControllerAssault::DoWincheck()
{
	if(
		m_GameOverTick == -1 &&
		m_AssaultOverTick == -1 &&
		!m_Warmup &&
		!GameServer()->m_World.m_ResetRequested)
	{
		// as soon as the assault team touches the flag, it's game over
		CFlag *F = m_pFlag;
		if (F && F->m_pCarryingCharacter)
		{
			EndAssault(true);
		}
		else
		{
			// or if time is up
			int TimeLimitTicks = g_Config.m_SvAssaultTimelimit * 60 * Server()->TickSpeed();
			if((Server()->Tick() - m_AssaultStartTick) >= TimeLimitTicks)
			{
				EndAssault(false);
			}
		}

	}
	else if(
		m_GameOverTick == -1 &&
		!m_Warmup &&
		!GameServer()->m_World.m_ResetRequested)
	{
		// check if it's time to end the entire round
		if(m_FinishedAllAssault)
		{
			if(Server()->Tick() > m_AssaultOverTick)
			{
				EndRound();
			}
		}
	}
}

bool CGameControllerAssault::CanSpawn(int Team, vec2 *pOutPos)
{
	// The reason it's (Team == m_AssaultTeam) and not != is because
	// we have already flipped m_AssaultTeam teams in EndAssault
	if (m_AssaultOverTick != -1 && Team == m_AssaultTeam)
	{
		return false;
	}
	else if (m_AssaultTeamSpawnDelay > 0 && Team == m_AssaultTeam)
	{
		// Assault Team Spawn Delay
		// give defense time to prepare
		return false;
	}
	else
	{
		CSpawnEval Eval;

		// spectators can't spawn
		if(Team == TEAM_SPECTATORS)
			return false;

		if(IsTeamplay())
		{
			Eval.m_FriendlyTeam = Team;

			// first try own team spawn, then normal spawn and then enemy, which
			// depends on what m_AssaultTeam is
			// look in IGameController::OnEntity() at the cases for spawns
			EvaluateSpawnType(&Eval, 1 + !(Team ^ m_AssaultTeam));
			if(!Eval.m_Got)
			{
				EvaluateSpawnType(&Eval, 0);
				if(!Eval.m_Got)
					EvaluateSpawnType(&Eval, 1 + (Team ^ m_AssaultTeam));
			}
		}
		else
		{
			EvaluateSpawnType(&Eval, 0);
			EvaluateSpawnType(&Eval, 1);
			EvaluateSpawnType(&Eval, 2);
		}

		*pOutPos = Eval.m_Pos;
		return Eval.m_Got;
	}
}

void CGameControllerAssault::StartRound()
{
	IGameController::StartRound();
	m_FinishedAllAssault = false;
	m_aCaptureTime[0] = -1.0f;
	m_aCaptureTime[1] = -1.0f;
	// based on default in constructor
	m_AssaultTeam = TEAM_BLUE;
	// reset AssaultTotalScores and other stuff
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_AssaultTotalScore = 0;
			GameServer()->m_apPlayers[i]->m_AssaultCapturedFlagTeam = -1;
		}
	}
	if (g_Config.m_SvAssaultTeamSpawnDelay > 0)
	{
		m_AssaultTeamSpawnDelay = g_Config.m_SvAssaultTeamSpawnDelay * Server()->TickSpeed();
	}
	else
	{
		m_AssaultTeamSpawnDelay = -1;
	}
	StartAssault(false);
}

void CGameControllerAssault::EndRound()
{
	IGameController::EndRound();

	// we have to do this so the client knows which team won (which ever has higher teamscore)
	if (m_aCaptureTime[TEAM_RED] < m_aCaptureTime[TEAM_BLUE])
	{
			m_aTeamscore[TEAM_RED] = 1;
			m_aTeamscore[TEAM_BLUE] = 0;
	}
	else if (m_aCaptureTime[TEAM_BLUE] < m_aCaptureTime[TEAM_RED])
	{
		m_aTeamscore[TEAM_RED] = 0;
		m_aTeamscore[TEAM_BLUE] = 1;
	}
	else
	{
		// they are equal o_o
		m_aTeamscore[TEAM_RED] = 1;
		m_aTeamscore[TEAM_BLUE] = 1;	
	}

	// check for exceptions
	if (m_aCaptureTime[TEAM_RED] == -2.0f)
	{
		m_aTeamscore[TEAM_RED] = -1;
	}
	if (m_aCaptureTime[TEAM_BLUE] == -2.0f)
	{
		m_aTeamscore[TEAM_BLUE] = -1;
	}

	// announce times
	GameServer()->SendChat(-1, -2, "┎─────────────────────");
	if (m_aCaptureTime[TEAM_BLUE] == -2.0f)
	{
		// they were not able to capture the flag before time was up
		GameServer()->SendChat(-1, -2, "┃ The blue team failed to");
		GameServer()->SendChat(-1, -2, "┃ capture the flag");
	}
	else
	{
		// they captured the flag
		GameServer()->SendChat(-1, -2, "┃ Blue team capture time:");
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "┃ %s%d:%s%d.%s%d",
			((int)m_aCaptureTime[TEAM_BLUE] / 60) < 10 ? "0" : "",
			(int)m_aCaptureTime[TEAM_BLUE] / 60,
			((int)m_aCaptureTime[TEAM_BLUE] % 60) < 10 ? "0" : "",
			(int)m_aCaptureTime[TEAM_BLUE] % 60,
			((int)(m_aCaptureTime[TEAM_BLUE] * 100) % 100) < 10 ? "0" : "",
			(int)(m_aCaptureTime[TEAM_BLUE] * 100) % 100);
		GameServer()->SendChat(-1, -2, aBuf);
	}
	GameServer()->SendChat(-1, -2, "┠─────────────────────");
	if (m_aCaptureTime[TEAM_RED] == -2.0f)
	{
		// they were not able to capture the flag before time was up
		GameServer()->SendChat(-1, -2, "┃ The red team failed to");
		GameServer()->SendChat(-1, -2, "┃ capture the flag");
	}
	else
	{
		// they captured the flag
		GameServer()->SendChat(-1, -2, "┃ Red team capture time:");
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "┃ %s%d:%s%d.%s%d",
			((int)m_aCaptureTime[TEAM_RED] / 60) < 10 ? "0" : "",
			(int)m_aCaptureTime[TEAM_RED] / 60,
			((int)m_aCaptureTime[TEAM_RED] % 60) < 10 ? "0" : "",
			(int)m_aCaptureTime[TEAM_RED] % 60,
			((int)(m_aCaptureTime[TEAM_RED] * 100) % 100) < 10 ? "0" : "",
			(int)(m_aCaptureTime[TEAM_RED] * 100) % 100);
		GameServer()->SendChat(-1, -2, aBuf);
	}
	GameServer()->SendChat(-1, -2, "┖─────────────────────");

	// set scores to AssaultTotalScores
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_Score = GameServer()->m_apPlayers[i]->m_AssaultTotalScore;
		}
	}
}

void CGameControllerAssault::SetAssaultFlags()
{
	// remove old flag
	if (m_pFlag)
	{
		GameServer()->m_World.RemoveEntity(m_pFlag);
	}

	// insert new flag
	m_pFlag = new CFlag(&GameServer()->m_World, m_AssaultTeam ^ 1);
	m_pFlag->m_StandPos = m_FlagPosition;
	m_pFlag->m_Pos = m_FlagPosition;
	GameServer()->m_World.InsertEntity(m_pFlag);	
}

void CGameControllerAssault::StartAssault(bool ResetWorld)
{

	if (m_AssaultTeamSpawnDelay > 0)
	{
		// we will call StartAssault once the assault team spawns
		return;
	}
	// else if (m_AssaultTeamSpawnDelay == -1 && m_aCaptureTime[m_AssaultTeam ^ 1] != -1.0f && m_AssaultOverTick != -1)
	// {
	// 	// we are about to start the second round and we have to reset the spawn delay timer
	// 	// m_AssaultTeamSpawnDelay = g_Config.m_SvAssaultTeamSpawnDelay * Server()->TickSpeed();
	// 	return;
	// }

	if (ResetWorld)
	{
		ResetGame();
		Server()->DemoRecorder_HandleAutoStart();
	}

	// autofail - set timer to first assault team's flag cap time
	if (!m_FinishedAllAssault)
	{
		// another way of saying "if it's the second assault round"
		if (m_aCaptureTime[m_AssaultTeam ^ 1] != -1.0f)
		{
			int TimeLimitTicks = g_Config.m_SvAssaultTimelimit * 60 * Server()->TickSpeed();
			if (m_aCaptureTime[m_AssaultTeam ^ 1] == -2.0f)
			{
				// first assault team failed to capture the flag
				// this gives the second assault team the full amount allotted by sv_assault_timelimit
				m_AssaultStartTick = Server()->Tick();
			}
			else
			{
				int FirstCaptureTicks = (int)(m_aCaptureTime[m_AssaultTeam ^ 1] * Server()->TickSpeed());
				// because: Timer = Timelimit - (Tick - StartTick)
				// and: Timer = CaptureTicks
				// then: StartTick = (CaptureTicks - Timelimit) + Tick
				m_AssaultStartTick = (FirstCaptureTicks - TimeLimitTicks) + Server()->Tick();
			}

			GameServer()->SendChat(-1, m_AssaultTeam, "┎─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam, "┃ Round 2: Attack");
			GameServer()->SendChat(-1, m_AssaultTeam, "┖─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam, "‣ Capture the flag before time is up");

			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┎─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┃ Round 2: Defend");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┖─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "‣ Defend the flag until time is up");
		}
		else
		{
			m_AssaultStartTick = Server()->Tick();

			GameServer()->SendChat(-1, m_AssaultTeam, "┎─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam, "┃ Round 1: Attack");
			GameServer()->SendChat(-1, m_AssaultTeam, "┖─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam, "‣ Capture the flag as fast as you can");
			GameServer()->SendChat(-1, m_AssaultTeam, "‣ Your capture time determines the time you need to defend in Round 2 ");

			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┎─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┃ Round 1: Defend");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "┖─────────────────────");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "‣ Defend the flag as long as you can");
			GameServer()->SendChat(-1, m_AssaultTeam ^ 1, "‣ The time you defend determines your time limit for attack in Round 2");
		}
	}

	m_AssaultAbsoluteStartTick = Server()->Tick();
	m_AssaultOverTick = -1;
	SetAssaultFlags();
}

void CGameControllerAssault::EndAssault(bool CapturedFlag)
{
	if (m_aCaptureTime[m_AssaultTeam] == -1.0f)
	{
		// record tick variables
		m_AssaultOverTick = Server()->Tick();

		if (CapturedFlag)
		{
			// record capture time
			m_aCaptureTime[m_AssaultTeam] = (Server()->Tick() - m_AssaultAbsoluteStartTick) / (float)Server()->TickSpeed();

			// make pretty stuff around the flag
			for(int i = 0; i < 6; i++)
			{
				float angle = static_cast<float>(i) * 2.0 * pi / 6.0;
				vec2 PoofPos = m_FlagPosition + vec2(100.0 * cos(angle), 100.0 * sin(angle));
				GameServer()->CreatePlayerSpawn(PoofPos);
			}

			// kill everybody on the defending team
			for(CCharacter *p = (CCharacter*) GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter *)p->TypeNext())
			{
				if(p->GetPlayer()->GetTeam() != m_AssaultTeam)
				{
					GameServer()->CreateExplosion(p->m_Pos, -1, WEAPON_GAME, true);
					GameServer()->CreateSound(p->m_Pos, SOUND_GRENADE_EXPLODE);
					if(m_pFlag->m_pCarryingCharacter)
					{
						p->Die(m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID(), WEAPON_NINJA);
					}
				}
			}

			// remember the player that captured the flag
			m_pFlag->m_pCarryingCharacter->GetPlayer()->m_AssaultCapturedFlagTeam = m_AssaultTeam;
		}
		else
		{
			// -2 means no flag cap
			m_aCaptureTime[m_AssaultTeam] = -2.0f;
		}

		// record AssaultTotalScores
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (GameServer()->m_apPlayers[i])
			{
				GameServer()->m_apPlayers[i]->m_AssaultTotalScore += GameServer()->m_apPlayers[i]->m_Score;
			}
		}

		// announce assault capture and time
		if (m_aCaptureTime[m_AssaultTeam] == -2.0f)
		{
			// they were not able to capture the flag before time was up
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "The %s team failed to capture the flag",
				m_AssaultTeam ? "blue" : "red");
			GameServer()->SendChat(-1, -2, aBuf);
		}
		else
		{
			// they captured the flag
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "The %s team captured the flag in: %s%d:%s%d.%s%d",
				m_AssaultTeam ? "blue" : "red",
				((int)m_aCaptureTime[m_AssaultTeam] / 60) < 10 ? "0" : "",
				(int)m_aCaptureTime[m_AssaultTeam] / 60,
				((int)m_aCaptureTime[m_AssaultTeam] % 60) < 10 ? "0" : "",
				(int)m_aCaptureTime[m_AssaultTeam] % 60,
				((int)(m_aCaptureTime[m_AssaultTeam] * 100) % 100) < 10 ? "0" : "",
				(int)(m_aCaptureTime[m_AssaultTeam] * 100) % 100);
			GameServer()->SendChat(-1, -2, aBuf);
		}
	}

	// check if both teams have capture times
	m_FinishedAllAssault = true;
	for (int i = 0; i < 2; ++i)
	{
		if (m_aCaptureTime[i] == -1.0f)
		{
			m_FinishedAllAssault = false;
			break;
		}
	}

	// reset the spawn delay
	if (g_Config.m_SvAssaultTeamSpawnDelay > 0)
	{
		m_AssaultTeamSpawnDelay = g_Config.m_SvAssaultTeamSpawnDelay * Server()->TickSpeed();
	}
	else
	{
		m_AssaultTeamSpawnDelay = -1;
	}

	// switch assault teams
	m_AssaultTeam ^= 1;
}

bool CGameControllerAssault::CanBeMovedOnBalance(int ClientID)
{
	CCharacter* Character = GameServer()->m_apPlayers[ClientID]->GetCharacter();
	if(Character)
	{
		CFlag *F = m_pFlag;
		if(F && F->m_pCarryingCharacter == Character)
			return false;
	}
	return true;
}

void CGameControllerAssault::Snap(int SnappingClient)
{
	// IGameController::Snap(SnappingClient);
	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_AssaultStartTick;
	pGameInfoObj->m_WarmupTimer = GameServer()->m_World.m_Paused ? m_UnpauseTimer : m_Warmup;

	pGameInfoObj->m_ScoreLimit = g_Config.m_SvScorelimit;
	pGameInfoObj->m_TimeLimit = g_Config.m_SvAssaultTimelimit;

	pGameInfoObj->m_RoundNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount+1;


	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
	pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

	if (m_pFlag)
	{
		if(m_pFlag->m_Team == TEAM_RED)
		{
			if(m_pFlag->m_AtStand)
				pGameDataObj->m_FlagCarrierRed = FLAG_ATSTAND;
			else if(m_pFlag->m_pCarryingCharacter && m_pFlag->m_pCarryingCharacter->GetPlayer())
				pGameDataObj->m_FlagCarrierRed = m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID();
			else
				pGameDataObj->m_FlagCarrierRed = FLAG_TAKEN;

			pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
		}
		if(m_pFlag->m_Team == TEAM_BLUE)
		{
			if(m_pFlag->m_AtStand)
				pGameDataObj->m_FlagCarrierBlue = FLAG_ATSTAND;
			else if(m_pFlag->m_pCarryingCharacter && m_pFlag->m_pCarryingCharacter->GetPlayer())
				pGameDataObj->m_FlagCarrierBlue = m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID();
			else
				pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;

			pGameDataObj->m_FlagCarrierRed = FLAG_MISSING;
		}
	}
	else
	{
		pGameDataObj->m_FlagCarrierRed = FLAG_MISSING;
		pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
	}

	// at the end of the round, indicate which tees had captured a flag
	if (m_GameOverTick != -1)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_AssaultCapturedFlagTeam)
			{
				switch (GameServer()->m_apPlayers[i]->m_AssaultCapturedFlagTeam)
				{
					case TEAM_RED:
						pGameDataObj->m_FlagCarrierBlue = GameServer()->m_apPlayers[i]->GetCID();
						break;
					case TEAM_BLUE:
						pGameDataObj->m_FlagCarrierRed = GameServer()->m_apPlayers[i]->GetCID();
						break;	
				}
			}
		}
	}
}

void CGameControllerAssault::Tick()
{
	// do warmup
	if(!GameServer()->m_World.m_Paused && m_Warmup)
	{
		m_Warmup--;
		if(!m_Warmup)
			StartRound();
	}

	if(m_GameOverTick != -1)
	{
		// game over.. wait for restart
		if(Server()->Tick() > m_GameOverTick+Server()->TickSpeed()*10)
		{
			CycleMap();
			StartRound();
			m_RoundCount++;
		}
		return;
	}
	else if(GameServer()->m_World.m_Paused && m_UnpauseTimer)
	{
		--m_UnpauseTimer;
		if(!m_UnpauseTimer)
			GameServer()->m_World.m_Paused = false;
	}

	// game is Paused
	if(GameServer()->m_World.m_Paused)
	{
		++m_RoundStartTick;
		++m_AssaultStartTick;
	}

	// note: we also check this in DoWinCheck() to see if it's time to EndRound()
	if (!m_FinishedAllAssault)
	{
		// if m_AssaultTeamSpawnDelay is -1, then it means it's disabled. Ignore it.
		if (m_AssaultTeamSpawnDelay != -1)
		{
			// don't factor assault team spawn delay into time calculation
			if(m_AssaultTeamSpawnDelay > 0)
			{
				if(m_AssaultTeamSpawnDelay == g_Config.m_SvAssaultTeamSpawnDelay * Server()->TickSpeed())
				{
					// should trigger only once
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "You will spawn in %d seconds", m_AssaultTeamSpawnDelay / Server()->TickSpeed());
					GameServer()->SendChat(-1, m_AssaultTeam, aBuf);
					str_format(aBuf, sizeof(aBuf), "The round will begin in %d seconds", m_AssaultTeamSpawnDelay / Server()->TickSpeed());
					GameServer()->SendChat(-1, m_AssaultTeam ^ 1, aBuf);
					m_AssaultStartTick = Server()->Tick();
				}
				--m_AssaultTeamSpawnDelay;
				++m_AssaultStartTick;
				++m_RoundStartTick;
			}
			else if (m_AssaultTeamSpawnDelay == 0)
			{
				m_AssaultTeamSpawnDelay = -1;
				StartAssault(false);
			}
		}
		else
		{
			// no spawn delay - we will just handle starting the next asault round based on the m_AssaultOverTick
			if(m_AssaultOverTick != -1)
			{
				// assault over.. wait for restart
				if(Server()->Tick() > m_AssaultOverTick)
				{
					StartAssault();
				}
				else
				{
					char aBuf[64];
					str_format(aBuf, sizeof aBuf, "Switching teams in %d seconds",
						5 - ((Server()->Tick() - m_AssaultOverTick) / Server()->TickSpeed()));
					m_Broadcast.Update(-1, aBuf, 1);
				}
			}
		}
	}


	// call this only on the first tick
	if(!m_AssaultInitialized && !GameServer()->m_World.m_Paused)
	{
		// I don't like how StartAssault() doesn't get called at the start of the server
		// so I'm doing it now
		StartAssault(false);
		m_AssaultInitialized = true;
		// SetAssaultFlags();
	}

	// do team-balancing
	if(IsTeamplay() && m_UnbalancedTick != -1 && Server()->Tick() > m_UnbalancedTick+g_Config.m_SvTeambalanceTime*Server()->TickSpeed()*60)
	{
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Balancing teams");

		int aT[2] = {0,0};
		float aTScore[2] = {0,0};
		float aPScore[MAX_CLIENTS] = {0.0f};
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				aT[GameServer()->m_apPlayers[i]->GetTeam()]++;
				aPScore[i] = GameServer()->m_apPlayers[i]->m_Score*Server()->TickSpeed()*60.0f/
					(Server()->Tick()-GameServer()->m_apPlayers[i]->m_ScoreStartTick);
				aTScore[GameServer()->m_apPlayers[i]->GetTeam()] += aPScore[i];
			}
		}

		// are teams unbalanced?
		if(absolute(aT[0]-aT[1]) >= 2)
		{
			int M = (aT[0] > aT[1]) ? 0 : 1;
			int NumBalance = absolute(aT[0]-aT[1]) / 2;

			do
			{
				CPlayer *pP = 0;
				float PD = aTScore[M];
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!GameServer()->m_apPlayers[i] || !CanBeMovedOnBalance(i))
						continue;
					// remember the player who would cause lowest score-difference
					if(GameServer()->m_apPlayers[i]->GetTeam() == M && (!pP || absolute((aTScore[M^1]+aPScore[i]) - (aTScore[M]-aPScore[i])) < PD))
					{
						pP = GameServer()->m_apPlayers[i];
						PD = absolute((aTScore[M^1]+aPScore[i]) - (aTScore[M]-aPScore[i]));
					}
				}

				// move the player to the other team
				int Temp = pP->m_LastActionTick;
				pP->SetTeam(M^1);
				pP->m_LastActionTick = Temp;

				pP->Respawn();
				pP->m_ForceBalanced = true;
			} while (--NumBalance);

			m_ForceBalanced = true;
		}
		m_UnbalancedTick = -1;
	}

	// check for inactive players
	if(g_Config.m_SvInactiveKickTime > 0)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
		#ifdef CONF_DEBUG
			if(g_Config.m_DbgDummies)
			{
				if(i >= MAX_CLIENTS-g_Config.m_DbgDummies)
					break;
			}
		#endif
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !Server()->IsAuthed(i))
			{
				if(Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick+g_Config.m_SvInactiveKickTime*Server()->TickSpeed()*60)
				{
					switch(g_Config.m_SvInactiveKick)
					{
					case 0:
						{
							// move player to spectator
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 1:
						{
							// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
							int Spectators = 0;
							for(int j = 0; j < MAX_CLIENTS; ++j)
								if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
									++Spectators;
							if(Spectators >= g_Config.m_SvSpectatorSlots)
								Server()->Kick(i, "Kicked for inactivity");
							else
								GameServer()->m_apPlayers[i]->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 2:
						{
							// kick the player
							Server()->Kick(i, "Kicked for inactivity");
						}
					}
				}
			}
		}
	}

	DoBroadcasts();

	DoWincheck();

	if(GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;

	// flag interactions
	CFlag *F = m_pFlag;

	if(!F)
		return;

	// flag hits death-tile or left the game layer, reset it
	if(GameServer()->Collision()->GetCollisionAt(F->m_Pos.x, F->m_Pos.y)&CCollision::COLFLAG_DEATH || F->GameLayerClipped(F->m_Pos))
	{
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
		GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
		F->Reset();
		return;
	}

	//
	if(F->m_pCarryingCharacter)
	{
		// update flag position
		F->m_Pos = F->m_pCarryingCharacter->m_Pos;
	}
	else
	{
		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(F->m_Pos, CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->m_Pos, apCloseCCharacters[i]->m_Pos, NULL, NULL))
				continue;

			if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == F->m_Team)
			{
				// return the flag
				if(!F->m_AtStand)
				{
					CCharacter *pChr = apCloseCCharacters[i];
					pChr->GetPlayer()->m_Score += 1;

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s'",
						pChr->GetPlayer()->GetCID(),
						Server()->ClientName(pChr->GetPlayer()->GetCID()));
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
					F->Reset();
				}
			}
			else
			{
				// take the flag
				if(F->m_AtStand)
				{
					// m_aTeamscore[m_pFlag->m_Team^1]++;
					F->m_GrabTick = Server()->Tick();
				}

				F->m_AtStand = 0;
				F->m_pCarryingCharacter = apCloseCCharacters[i];
				F->m_pCarryingCharacter->GetPlayer()->m_Score += 1;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s'",
					F->m_pCarryingCharacter->GetPlayer()->GetCID(),
					Server()->ClientName(F->m_pCarryingCharacter->GetPlayer()->GetCID()));
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					CPlayer *pPlayer = GameServer()->m_apPlayers[c];
					if(!pPlayer)
						continue;

					if(pPlayer->GetTeam() == TEAM_SPECTATORS && pPlayer->m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[pPlayer->m_SpectatorID] && GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetTeam() == m_pFlag->m_Team)
						GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
					else if(pPlayer->GetTeam() == m_pFlag->m_Team)
						GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
					else
						GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, c);
				}
				// demo record entry
				GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, -2);
				break;
			}
		}

		if(!F->m_pCarryingCharacter && !F->m_AtStand)
		{
			if(Server()->Tick() > F->m_DropTick + Server()->TickSpeed()*30)
			{
				GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
				F->Reset();
			}
			else
			{
				F->m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
				GameServer()->Collision()->MoveBox(&F->m_Pos, &F->m_Vel, vec2(F->ms_PhysSize, F->ms_PhysSize), 0.5f);
			}
		}
	}
}

void CGameControllerAssault::DoBroadcasts()
{
	if (m_GameOverTick != -1)
		return;

	m_Broadcast.Operate();
}
