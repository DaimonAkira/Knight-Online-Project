#include "stdafx.h"
#include "Map.h"
#include "AISocket.h"
#include "Npc.h"
#include "MagicProcess.h"

using std::string;

bool CAISocket::HandlePacket(Packet & pkt)
{
	uint8 bCommand = pkt.GetOpcode();

	switch (bCommand)
	{
	case AG_CHECK_ALIVE_REQ:
		RecvCheckAlive(pkt);
		break;

	case AI_SERVER_CONNECT:
		LoginProcess(pkt);
		break;

	case AG_SERVER_INFO:
		RecvServerInfo(pkt);
		break;

	case NPC_INFO_ALL:
		RecvNpcInfoAll(pkt);
		break;

	case MOVE_RESULT:
		RecvNpcMoveResult(pkt);
		break;

	case MOVE_END_RESULT:
		break;

	case AG_ATTACK_REQ:
		RecvNpcAttack(pkt);
		break;

	case AG_MAGIC_ATTACK_REQ:
		CMagicProcess::MagicPacket(pkt);
		break;

	case AG_NPC_INFO:
		RecvNpcInfo(pkt);
		break;

	case AG_NPC_REGION_UPDATE:
		RecvNpcRegionUpdate(pkt);
		break;

	case AG_USER_EXP:
		RecvUserExp(pkt);
		break;

	case AG_SYSTEM_MSG:
		RecvSystemMsg(pkt);
		break;

	case AG_NPC_GIVE_ITEM:
		RecvNpcGiveItem(pkt);
		break;

	case AG_NPC_GATE_DESTORY:
		RecvGateDestory(pkt);
		break;

	case AG_DEAD:
		RecvNpcDead(pkt);
		break;

	case AG_NPC_INOUT:
		RecvNpcInOut(pkt);
		break;

	case AG_BATTLE_EVENT:
		RecvBattleEvent(pkt);
		break;

	case AG_NPC_EVENT_ITEM:
		RecvNpcEventItem(pkt);
		break;

	case AG_NPC_GATE_OPEN:
		RecvGateOpen(pkt);
		break;

	case AG_COMPRESSED:
		RecvCompressed(pkt);
		break;

	case AG_NPC_HP_CHANGE:
		RecvNpcHpChange(pkt);
		break;
	}

	return true;
}

void CAISocket::OnConnect()
{
	KOSocket::OnConnect();
	m_bHasConnected = true; 
}

void CAISocket::OnDisconnect()
{
	g_pMain->DeleteAllNpcList();
	g_pMain->m_sErrorSocketCount = 3;
}

void CAISocket::LoginProcess(Packet & pkt)
{
	uint8 bReconnect = pkt.read<uint8>();

	if (bReconnect == 1)

	g_pMain->m_bServerCheckFlag = true;
	g_pMain->SendAllUserInfo();
}

void CAISocket::RecvServerInfo(Packet & pkt)
{
	int size = g_pMain->m_ZoneArray.GetSize();
	uint16 sTotalMonster;
	uint8 bZone;

	pkt >> bZone >> sTotalMonster;

	g_pMain->m_sZoneCount++;

	if (g_pMain->m_sZoneCount == size)
	{
		g_pMain->m_sZoneCount = 0;
		g_pMain->m_bFirstServerFlag = true;
		g_pMain->m_bPointCheckFlag = true;
		g_pMain->m_NationMonumentInformationArray.DeleteAllData();		

		if (g_pMain->m_MonsterRespawnListInformationArray.GetSize() > 0)
		{
			foreach_stlmap_nolock (itr, g_pMain->m_MonsterRespawnListInformationArray)
			{
				if (itr->second->ZoneID != ZONE_CHAOS_DUNGEON && itr->second->ZoneID != ZONE_BORDER_DEFENSE_WAR && itr->second->ZoneID != ZONE_JURAD_MOUNTAIN)
				{
					g_pMain->SpawnEventNpc(itr->second->sSid,true,itr->second->ZoneID,itr->second->X,itr->second->Y,itr->second->Z,itr->second->sCount,itr->second->bRadius);
					g_pMain->SpawnEventNpc(itr->second->sSid,false,itr->second->ZoneID,itr->second->X,itr->second->Y,itr->second->Z,itr->second->sCount,itr->second->bRadius);
				}
			}
		}
		color_c("", FOREGROUND_INTENSITY +  0x7);
		printf("AIServer is connected GameServer Server IP - [%s]\n", GetRemoteIP().c_str());
	}
}

void CAISocket::RecvNpcInfoAll(Packet & pkt)
{
	uint8 bCount = pkt.read<uint8>();

	pkt.SByte();
	for (int i = 0; i < bCount; i++)
	{
		uint8 bDirection;
		std::string strName;

		CNpc* pNpc = new CNpc();
		pNpc->Initialize();

		pkt >> pNpc->m_NpcState >> pNpc->m_sNid >> pNpc->m_sSid >> pNpc->m_sPid >> pNpc->m_sSize >> pNpc->m_iWeapon_1 >> pNpc->m_iWeapon_2
			>> pNpc->m_bZone >> strName >> pNpc->m_bNation >> pNpc->m_bLevel
			>> pNpc->m_curx >> pNpc->m_curz >> pNpc->m_cury >> bDirection
			>> pNpc->m_tNpcType >> pNpc->m_iSellingGroup >> pNpc->m_iMaxHP >> pNpc->m_iHP >> pNpc->m_byGateOpen
			>> pNpc->m_fTotalHitrate >> pNpc->m_fTotalEvasionrate 
			>> pNpc->m_sTotalAc >> pNpc->m_sTotalHit 
			>> pNpc->m_byObjectType
			>> pNpc->m_byTrapNumber >> pNpc->m_bMonster >> pNpc->m_oSocketID >> pNpc->m_bEventRoom
			>> pNpc->m_sFireR >> pNpc->m_sColdR >> pNpc->m_sLightningR
			>> pNpc->m_sMagicR >> pNpc->m_sDiseaseR >> pNpc->m_sPoisonR;

		if (strName.empty())
			strName = "<the spawn with no name>";

		if (strName.length() > MAX_NPC_SIZE)
		{
			pNpc->DecRef();
			continue;
		}

		pNpc->m_pMap = g_pMain->GetZoneByID(pNpc->GetZoneID());
		if (pNpc->GetMap() == nullptr)
		{
			pNpc->DecRef();
			continue;
		}

		pNpc->m_strName = strName;

		pNpc->m_byDirection = bDirection;
		pNpc->SetRegion(pNpc->GetNewRegionX(), pNpc->GetNewRegionZ());

		if (pNpc->m_byObjectType == SPECIAL_OBJECT)
		{
			_OBJECT_EVENT* pEvent = pNpc->GetMap()->GetObjectEvent(pNpc->m_sSid);
			if (pEvent != nullptr)
				pEvent->byLife = 1;
		}

		if (!g_pMain->m_arNpcArray.PutData(pNpc->GetID(), pNpc))
		{
			pNpc->DecRef();
			continue;
		}

		if (pNpc->m_NpcState == NPC_DEAD)
		{
			continue;
		}

		pNpc->SendInOut(INOUT_IN, pNpc->GetX(), pNpc->GetZ(), pNpc->GetY());
	}
}

void CAISocket::RecvNpcMoveResult(Packet & pkt)
{
	uint8 flag;	
	uint16 sNid;
	float fX, fY, fZ, fSecForMetor;
	pkt >> flag >> sNid >> fX >> fZ >> fY >> fSecForMetor;

	CNpc * pNpc = g_pMain->GetNpcPtr(sNid);
	if (pNpc == nullptr)
		return;

	if (pNpc->isDead())
	{
		Packet result(AG_NPC_HP_REQ);
		result << sNid << pNpc->m_iHP;
		Send(&result);
	}

	pNpc->MoveResult(fX, fY, fZ, fSecForMetor);
}

void CAISocket::RecvNpcAttack(Packet & pkt)
{
    CNpc * pAttacker;
    Unit * pTarget;
    uint16 sAttackerID, sTargetID;
    int16 sDamage;
    uint8 bResult = ATTACK_FAIL;

    pkt >> sAttackerID >> sTargetID;

    pAttacker = g_pMain->GetNpcPtr(sAttackerID);
    pTarget = g_pMain->GetUnitPtr(sTargetID);

    if (pAttacker == nullptr
        || pAttacker->isPlayer()
        || pTarget == nullptr
        || pAttacker->isDead()
        || pTarget->isDead())
        return;

    sDamage = pAttacker->GetDamage(pTarget);
    if (sDamage > 0)
    {
        pTarget->HpChange(-(sDamage), pAttacker);
        if (pTarget->isDead())
            bResult = ATTACK_TARGET_DEAD;
        else
            bResult = ATTACK_SUCCESS;

        if (pTarget->isPlayer())
            TO_USER(pTarget)->ItemWoreOut(DEFENCE, sDamage);
    }

    Packet result(WIZ_ATTACK, uint8(LONG_ATTACK));
    result << bResult << sAttackerID << sTargetID;
    pAttacker->SendToRegion(&result);
}

void CAISocket::RecvNpcInfo(Packet & pkt)
{
	std::string strName;
	uint8 Mode, byDirection;
	uint16 sNid;
	bool bCreated = false;

	pkt.SByte();
	pkt >> Mode >> sNid;

	CNpc *pNpc = g_pMain->GetNpcPtr(sNid);
	if (pNpc == nullptr)
	{
		pNpc = new CNpc();
		pNpc->m_sNid = sNid;
		bCreated = true;
	}

	pkt >> pNpc->m_sSid >> pNpc->m_sPid >> pNpc->m_sSize >> pNpc->m_iWeapon_1 >> pNpc->m_iWeapon_2
		>> pNpc->m_bZone >> strName >> pNpc->m_bNation >> pNpc->m_bLevel
		>> pNpc->m_curx >> pNpc->m_curz >> pNpc->m_cury >> byDirection
		>> pNpc->m_tNpcType >> pNpc->m_iSellingGroup >> pNpc->m_iMaxHP >> pNpc->m_iHP >> pNpc->m_byGateOpen
		>> pNpc->m_fTotalHitrate >> pNpc->m_fTotalEvasionrate 
		>> pNpc->m_sTotalAc >> pNpc->m_sTotalHit 
		>> pNpc->m_byObjectType
		>> pNpc->m_byTrapNumber >> pNpc->m_bMonster >> pNpc->m_oSocketID >> pNpc->m_bEventRoom
		>> pNpc->m_sFireR >> pNpc->m_sColdR >> pNpc->m_sLightningR
		>> pNpc->m_sMagicR >> pNpc->m_sDiseaseR >> pNpc->m_sPoisonR;

	if (strName.empty() || strName.length() > MAX_NPC_SIZE)
	{
		pNpc->DecRef();
		return;
	}

	pNpc->m_NpcState = Mode;
	pNpc->m_byDirection = byDirection;
	pNpc->m_strName = strName;

	pNpc->m_pMap = g_pMain->GetZoneByID(pNpc->GetZoneID());
	if (pNpc->GetMap() == nullptr)
	{
		pNpc->DecRef();
		return;
	}

	pNpc->RegisterRegion();

	if (pNpc->m_byObjectType == SPECIAL_OBJECT)
	{
		_OBJECT_EVENT *pEvent = pNpc->GetMap()->GetObjectEvent( pNpc->m_sSid );
		if (pEvent != nullptr)
			pEvent->byLife = 1;
	}

	if (bCreated)
		g_pMain->m_arNpcArray.PutData(pNpc->GetID(), pNpc);

	if (pNpc->m_NpcState == NPC_DEAD)
	{
		return;
	}

	pNpc->SendInOut(INOUT_IN, pNpc->GetX(), pNpc->GetZ(), pNpc->GetY());
}

void CAISocket::RecvNpcRegionUpdate(Packet & pkt)
{
	uint16 sNpcID;
	float fX, fY, fZ;

	pkt >> sNpcID >> fX >> fY >> fZ;

	CNpc * pNpc = g_pMain->GetNpcPtr(sNpcID);
	if (pNpc == nullptr)
		return;

	pNpc->SetPosition(fX, fY, fZ);
	pNpc->RegisterRegion();
}

void CAISocket::RecvUserExp(Packet & pkt)
{
	uint16 tid;
	pkt >> tid;

	CUser* pUser = g_pMain->GetUserPtr(tid);
	if (pUser == nullptr)
		return;

	pUser->RecvUserExp(pkt);
}

void CAISocket::RecvSystemMsg(Packet & pkt)
{
	Packet result;
	std::string strSysMsg;
	uint8 bType;

	pkt >> bType >> strSysMsg;

	ChatPacket::Construct(&result, bType, &strSysMsg);
	g_pMain->Send_All(&result);
}

void CAISocket::RecvNpcGiveItem(Packet & pkt)
{
	Packet result(WIZ_ITEM_DROP);
	short sUid, sNid, regionx, regionz;
	float fX, fZ, fY;
	uint8 byCount, bZone;
	int nItemNumber[NPC_HAVE_ITEM_LIST];
	short sCount[NPC_HAVE_ITEM_LIST];
	CUser* pUser = nullptr;
	pkt >> sUid >> sNid >> bZone >> regionx >> regionz >> fX >> fZ >> fY >> byCount;

	for (int i = 0; i < byCount; i++)
		pkt >> nItemNumber[i] >> sCount[i];

	if (sUid < 0 || sUid >= MAX_USER)
		return;

	C3DMap *pMap = g_pMain->GetZoneByID(bZone);
	if (pMap == nullptr)
		return;

	_LOOT_BUNDLE * pBundle = new _LOOT_BUNDLE;

	pBundle->tDropTime = UNIXTIME;
	pBundle->x = fX;
	pBundle->z = fZ;
	pBundle->y = fY;

	for (int i = 0; i < byCount; i++)
	{
		if (g_pMain->GetItemPtr(nItemNumber[i]))
		{
			_LOOT_ITEM pItem(nItemNumber[i], sCount[i]);
			if (nItemNumber[i] == ITEM_GOLD)
			{
				uint32 coinAmount = sCount[i] * (100 + g_pMain->m_byCoinEventAmount) / 100;
				if (sCount[i] + coinAmount > USHRT_MAX)
					coinAmount = USHRT_MAX;

				pItem.sCount = coinAmount;
			}

			pBundle->Items.push_back(pItem);
		}
	}

	if (!pMap->RegionItemAdd(regionx, regionz, pBundle))
	{
		delete pBundle;
		return;
	}

	pUser = g_pMain->GetUserPtr(sUid);
	if (pUser == nullptr) 
		return;

	result << sNid << pBundle->nBundleID << uint8(2);
	if (!pUser->isInParty())
		pUser->Send(&result);
	else
		g_pMain->Send_PartyMember(pUser->GetPartyID(), &result);
}

void CAISocket::InitEventMonster(int index)
{
	int count = index;
	if( count < 0 || count > NPC_BAND )	{
		return;
	}

	int max_eventmop = count+EVENT_MONSTER;
	for( int i=count; i<max_eventmop; i++ )	{
		CNpc* pNpc = new CNpc();
		pNpc->m_sNid = i+NPC_BAND;
		if (!g_pMain->m_arNpcArray.PutData(pNpc->GetID(), pNpc))
		{
			pNpc->DecRef();
		}	
	}

	count = g_pMain->m_arNpcArray.GetSize();
}

void CAISocket::RecvCheckAlive(Packet & pkt)
{
	Packet result(AG_CHECK_ALIVE_REQ);
	g_pMain->m_sErrorSocketCount = 0;
	Send(&result);
}

void CAISocket::RecvGateDestory(Packet & pkt)
{
	uint16 nid, sCurZone, rX, rZ;
	bool bGateStatus;
	pkt >> nid >> bGateStatus >> sCurZone >> rX >> rZ;

	CNpc* pNpc = g_pMain->GetNpcPtr(nid);
	if (pNpc == nullptr)
		return;

	pNpc->m_byGateOpen = bGateStatus;
}

void CAISocket::RecvNpcDead(Packet & pkt)
{
	CNpc * pNpc;
	Unit * pAttacker;
	uint16 nid, attackerID;
	pkt >> nid >> attackerID;

	pNpc = g_pMain->GetNpcPtr(nid);
	if (pNpc == nullptr
		|| pNpc->GetMap() == nullptr)
		return;

	pAttacker = g_pMain->GetUnitPtr(attackerID);
	pNpc->OnDeath(pAttacker);
}

void CAISocket::RecvNpcInOut(Packet & pkt)
{
	uint8 bType;
	uint16 sNid;
	float fX, fZ, fY;

	pkt >> bType >> sNid >> fX >> fZ >> fY;
	CNpc * pNpc = g_pMain->GetNpcPtr(sNid);
	if (pNpc)
		pNpc->SendInOut(bType, fX, fZ, fY);
}

void CAISocket::RecvBattleEvent(Packet & pkt)
{
	string chatstr, strMaxUserName, strKnightsName;
	CUser* pUser = nullptr;
	CKnights* pKnights = nullptr;

	uint8 bType, bResult;
	pkt >> bType >> bResult;

	if (bType == BATTLE_EVENT_OPEN)
	{
	}
	else if (bType == BATTLE_MAP_EVENT_RESULT)
	{
		if (!g_pMain->isWarOpen())
		{
			return;
		}

		if (bResult == KARUS)
			g_pMain->m_byKarusOpenFlag = true;	
		else if (bResult == ELMORAD)
			g_pMain->m_byElmoradOpenFlag = true;
	}
		else if (bType == BATTLE_EVENT_RESULT)
	{
		if (!g_pMain->isWarOpen())
		{
			return;
		}

		pkt.SByte();
		pkt >> strMaxUserName;

		if (!strMaxUserName.empty()
			&& !g_pMain->m_byBattleSave)
		{
			Packet result(WIZ_BATTLE_EVENT, bType);
			result.SByte();
			result << bResult << strMaxUserName;

			g_pMain->AddDatabaseRequest(result);
			g_pMain->m_byBattleSave = true;
		}
	}
		else if (bType == BATTLE_EVENT_MAX_USER)
	{
		pkt.SByte();
		pkt >> strMaxUserName;

		if (!strMaxUserName.empty())
		{
			pUser = g_pMain->GetUserPtr(strMaxUserName, TYPE_CHARACTER);
			if (pUser != nullptr)
			{
				pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
				if (pKnights)
					strKnightsName = pKnights->m_strName;

				if (g_pMain->m_byBattleZone + ZONE_BATTLE_BASE == ZONE_BATTLE
					|| g_pMain->m_byBattleZone + ZONE_BATTLE_BASE == ZONE_BATTLE2
					|| g_pMain->m_byBattleZone + ZONE_BATTLE_BASE == ZONE_BATTLE6
					|| g_pMain->m_byBattleZone + ZONE_BATTLE_BASE == ZONE_BATTLE3)
				{
					if (pUser->GetNation() == KARUS)
						g_pMain->m_sKilledElmoNpc++;
					else
						g_pMain->m_sKilledKarusNpc++;

					if (g_pMain->m_sKilledKarusNpc == 3 || g_pMain->m_sKilledElmoNpc == 3)
					{
						g_pMain->m_bResultDelay = true;
						g_pMain->m_bResultDelayVictory = pUser->GetNation();
						g_pMain->BattleZoneResult(pUser->GetNation(), pUser);
					}
				}
			}
		}

		int nResourceID = 0;
		switch (bResult)
		{
		case 1:
			nResourceID = IDS_KILL_CAPTAIN;
			break;
		case 3:
			nResourceID = IDS_KILL_KARUS_GUARD1;
			break;
		case 4:
			nResourceID = IDS_KILL_KARUS_GUARD2;
			break;
		case 5:
			nResourceID = IDS_KILL_ELMO_GUARD1;
			break;
		case 6:
			nResourceID = IDS_KILL_ELMO_GUARD2;
			break;
		case 7:
			nResourceID = IDS_KILL_GATEKEEPER;
			break; 
		case 8:
			nResourceID = IDS_KILL_GATEKEEPER;
			break;
		}

		if (nResourceID == 0)
		{
			return;
		}

		g_pMain->GetServerResource(nResourceID, &chatstr, strKnightsName.c_str(), strMaxUserName.c_str());

		Packet result;
		string finalstr;

		g_pMain->GetServerResource(IDP_ANNOUNCEMENT, &finalstr, chatstr.c_str());
		ChatPacket::Construct(&result, WAR_SYSTEM_CHAT, &finalstr);
		g_pMain->Send_All(&result);

		ChatPacket::Construct(&result, PUBLIC_CHAT, &finalstr);
		g_pMain->Send_All(&result);
	}
}

void CAISocket::RecvNpcEventItem(Packet & pkt)
{
	uint16 sUid, sNid;
	uint32 nItemID, nCount;

	pkt >> sUid >> sNid >> nItemID >> nCount;

	CUser *pUser = g_pMain->GetUserPtr(sUid);
	if (pUser == nullptr)
		return;

	pUser->GiveItem(nItemID, nCount);
}

void CAISocket::RecvGateOpen(Packet & pkt)
{
	uint16 sNid, sEventID; 
	bool bFlag;

	pkt >> sNid >> sEventID >> bFlag;

	CNpc *pNpc = g_pMain->GetNpcPtr(sNid);
	if (pNpc == nullptr)	
	{
		return;
	}

	pNpc->m_byGateOpen = bFlag;

	_OBJECT_EVENT *pEvent = pNpc->GetMap()->GetObjectEvent(sEventID);
	if (pEvent == nullptr)	
	{
		return;
	}

	if (pNpc->isGate())
		pNpc->SendGateFlag(bFlag, false);
}

void CAISocket::RecvCompressed(Packet & pkt)
{
	uint32 compressedLength, originalLength;
	uint32 crc;
	pkt >> compressedLength >> originalLength >> crc;

	char *decompressedBuffer = new char[originalLength];

	uint32 result = lzf_decompress(pkt.contents() + pkt.rpos(), compressedLength, decompressedBuffer, originalLength);

	if (result
		!= originalLength)
	{
		delete [] decompressedBuffer;
		return;
	}

	pkt.Initialize(*decompressedBuffer);
	if (originalLength > 1)
		pkt.append(decompressedBuffer + 1, originalLength - 1);

	delete [] decompressedBuffer;

	HandlePacket(pkt);
}

void CAISocket::RecvNpcHpChange(Packet & pkt)
{
	Unit * pAttacker = nullptr;
	int16 nid, sAttackerID;
	int32 nHP, nAmount;

	pkt >> nid >> sAttackerID >> nHP >> nAmount;

	CNpc * pNpc = g_pMain->GetNpcPtr(nid);
	if (pNpc == nullptr)
		return;

	pAttacker = g_pMain->GetUnitPtr(sAttackerID);
	pNpc->HpChange(nAmount, pAttacker, false); 
}

void CAISocket::color_c(char* buff, WORD color)
{
	HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(hstdout, &csbi);
	SetConsoleTextAttribute(hstdout, color);
	WriteConsole(hstdout, buff, strlen(buff), new DWORD, 0);
}

void CAISocket::SendConsoleFAIL()
{
	color_c("[   ", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
	color_c("FAIL", FOREGROUND_RED + FOREGROUND_INTENSITY);
	color_c("   ]\n", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
}

void CAISocket::SendConsoleOK()
{
	color_c("[    ", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
	color_c("OK", FOREGROUND_GREEN + FOREGROUND_INTENSITY);
	color_c("    ]\n", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
}

void CAISocket::Yaz(char* yazi, WORD color)
{
	char data[105];
	memset(data, 0x00, 105);

	strcpy(data, yazi);

	int i2 = 0;
	i2 = 100 - strlen(yazi);

	for (int i = 0; i < i2; i++)
	sprintf(data, "%s ", data);

	if (color == 0)
	color_c((char*)(LPCTSTR)data, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
	else
	color_c((char*)(LPCTSTR)data, color);
}