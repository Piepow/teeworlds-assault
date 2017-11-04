#include <base/system.h>

#include <game/server/gamecontext.h>

#include "broadcaster.h"

#undef TS
#undef TICK
#undef GS

#define TS m_pGS->Server()->TickSpeed()
#define TICK m_pGS->Server()->Tick()
#define GS m_pGS
#define READYPLAYER(C) (((C) < 0 || (C) >= MAX_CLIENTS) ? 0 : (GS->IsClientReady(C) && GS->IsClientPlayer(C)) ? GS->m_apPlayers[C] : 0)

CBroadcaster::CBroadcaster(class CGameContext *pGameServer)
: m_pGS(pGameServer)
{
	Reset();
}

CBroadcaster::~CBroadcaster()
{
	Reset();
}

void CBroadcaster::SetDef(const char *pText)
{
	if (str_comp(m_aDefBroadcast, pText) == 0)
		return;

	str_copy(m_aDefBroadcast, pText, sizeof m_aDefBroadcast);
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (m_aBroadcastStop[i] < 0)
			str_copy(m_aBroadcast[i], pText, sizeof m_aBroadcast[i]); //this is unfortunately required
	m_Changed = ~0;
}

void CBroadcaster::Update(int Cid, const char *pText, int Lifespan)
{
	if (Cid < 0) // all
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			Update(i, pText, Lifespan);
		return;
	}

	m_aBroadcastStop[Cid] = Lifespan < 0 ? -1 : (TICK + Lifespan);
	bool Changed = str_comp(m_aBroadcast[Cid], pText) != 0;
	if (Changed)
	{
		str_copy(m_aBroadcast[Cid], pText, sizeof m_aBroadcast[Cid]);
		m_Changed |= (1<<Cid);
	}
}

void CBroadcaster::Reset()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aBroadcast[i][0] = '\0';
		m_aNextBroadcast[i] = m_aBroadcastStop[i] = -1;
		m_Changed = ~0;
		if (READYPLAYER(i))
		{
			GS->SendBroadcast("", i);
		}
	}
	m_aDefBroadcast[0] = '\0';
}

void CBroadcaster::Operate()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!GS->IsClientReady(i))
			continue;
		
		if (m_aBroadcastStop[i] >= 0 && m_aBroadcastStop[i] < TICK)
		{
			str_copy(m_aBroadcast[i], m_aDefBroadcast, sizeof m_aBroadcast[i]);
			if (!*m_aBroadcast[i])
			{
				GS->SendBroadcast(" ", i);
				m_Changed &= ~(1<<i);
			}
			else
			{
				m_Changed |= (1<<i);
			}
			m_aBroadcastStop[i] = -1;
		}

		if (((m_Changed & (1<<i)) || m_aNextBroadcast[i] < TICK) && *m_aBroadcast[i])
		{
			GS->SendBroadcast(m_aBroadcast[i], i);
			m_aNextBroadcast[i] = TICK + TS * 3;
		}
	}
	m_Changed = 0;
}
