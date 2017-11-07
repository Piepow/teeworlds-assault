/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_BASE_FLAG_DECOR_H
#define GAME_SERVER_ENTITIES_BASE_FLAG_DECOR_H

#include <game/server/entity.h>
#include "flag.h"

class CFlagDecor : public CEntity
{
public:
	enum
	{
		NUM_LASER_DOTS = 20
	};
public:
	CFlagDecor(CGameWorld *pGameWorld, CFlag *pBaseFlag);
	virtual ~CFlagDecor();

	virtual void Reset();
	virtual void Snap(int SnappingClient);

	void UpdatePos();

	class CFlag *m_pBaseFlag;
private:
	int m_aLaserDotIDs[NUM_LASER_DOTS];
	vec2 FlagPos;
};

#endif
