#ifndef GAME_SERVER_BROADCASTER_H
#define GAME_SERVER_BROADCASTER_H

#define MAX_BROADCAST 256
#define FORTEAMS(T) for(int T = TEAM_RED; T != -1; T = (T==TEAM_RED?TEAM_BLUE:-1))

class CBroadcaster
{
private:
	char m_aBroadcast[MAX_CLIENTS][MAX_BROADCAST];
	int m_aNextBroadcast[MAX_CLIENTS];
	int m_aBroadcastStop[MAX_CLIENTS];
	char m_aDefBroadcast[MAX_BROADCAST];

	int m_Changed;

	class CGameContext *m_pGS;
public:
	CBroadcaster(class CGameContext *pGameServer);
	virtual ~CBroadcaster();

	void SetDef(const char *pText);
	void Update(int Cid, const char *pText, int Lifespan);
	void Reset();
	void Operate();
};

#endif
