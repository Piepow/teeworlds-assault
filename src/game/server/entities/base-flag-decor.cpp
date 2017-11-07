/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "base-flag-decor.h"

CBaseFlagDecor::CBaseFlagDecor(CGameWorld *pGameWorld, CFlag *pBaseFlag)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_BASE_FLAG_DECOR), m_pBaseFlag(pBaseFlag)
{
	// base flag doesn't move so just set it once
	m_Pos = m_pBaseFlag->m_Pos + vec2(0.0f, -(28.0f));
	m_StartTick = Server()->Tick();
	GameWorld()->InsertEntity(this);

	for(int i = 0; i < NUM_LASER_DOTS; i++)
	{
		m_aLaserDotIDs[i] = Server()->SnapNewID();
	}
}

CBaseFlagDecor::~CBaseFlagDecor()
{
	for (int i = 0; i < NUM_LASER_DOTS; ++i)
	{
		Server()->SnapFreeID(m_aLaserDotIDs[i]);
	}
}

void CBaseFlagDecor::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CBaseFlagDecor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	// make a pretty ring of laser dots
	for(int i = 0; i < NUM_LASER_DOTS; i++)
	{
		float angle = static_cast<float>(i) * 2.0 * pi / NUM_LASER_DOTS;
		vec2 LaserPos = m_Pos + vec2(100.0 * cos(angle), 100.0 * sin(angle));

		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aLaserDotIDs[i], sizeof(CNetObj_Projectile)));
		if (pObj)
		{
			pObj->m_X = (int)LaserPos.x;
			pObj->m_Y = (int)LaserPos.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = m_StartTick;
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}
