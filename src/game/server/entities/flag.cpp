/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include "flag.h"
#include "flag-decor.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team, bool BaseFlag)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG)
{
	m_Team = Team;
	m_ProximityRadius = ms_PhysSize;
	m_pCarryingCharacter = NULL;
	m_GrabTick = 0;

	Reset();

	if (BaseFlag)
	{
		m_Decor = new CFlagDecor(GameWorld(), this);
		m_InitializeDecor = 0;
	}
	else
	{
		m_Decor = NULL;
		m_InitializeDecor = -1;
	}
}

void CFlag::Reset()
{
	m_pCarryingCharacter = NULL;
	m_AtStand = 1;
	m_Pos = m_StandPos;
	m_Vel = vec2(0,0);
	m_GrabTick = 0;
	if (m_Decor != NULL)
	{
		m_Decor->Reset();
	}
}

void CFlag::Tick()
{
	if (m_Decor != NULL)
	{
		if (m_InitializeDecor == 0)
		{
			m_Decor->Reset();
			m_InitializeDecor = 1;
		}
		if (!m_AtStand)
		{
			m_Decor->UpdatePos();
		}
	}
}

void CFlag::TickPaused()
{
	++m_DropTick;
	if(m_GrabTick)
		++m_GrabTick;
}

void CFlag::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if(!pFlag)
		return;

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
}

void CFlag::Destroy()
{
	if (m_Decor != NULL)
	{
		m_Decor->Destroy();
	}
}
