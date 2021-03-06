#include "stdafx.h"
#include "Map.h"
#include "KnightsManager.h"
#include "../shared/tstring.h"
#include "DBAgent.h"

void CKnightsManager::PacketProcess(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	uint8 opcode = pkt.read<uint8>();
	TRACE("Clan packet: %X\n", opcode); 
	switch (opcode)
	{
	case KNIGHTS_CREATE:
		CreateKnights(pUser, pkt);
		break;
	case KNIGHTS_JOIN:
		JoinKnights(pUser, pkt);
		break;
	case KNIGHTS_WITHDRAW:
		WithdrawKnights(pUser, pkt);
		break;
	case KNIGHTS_REMOVE:
	case KNIGHTS_ADMIT:
	case KNIGHTS_REJECT:
	case KNIGHTS_CHIEF:
	case KNIGHTS_VICECHIEF:
	case KNIGHTS_OFFICER:
	case KNIGHTS_PUNISH:
		ModifyKnightsMember(pUser, pkt, opcode);
		break;
	case KNIGHTS_HANDOVER_VICECHIEF_LIST:
		ModifyKnightsLeader(pUser, pkt, opcode);
		break;
	case KNIGHTS_HANDOVER_REQ:
		ModifyKnightsLeader(pUser, pkt, opcode);
		break;
	case KNIGHTS_HANDOVER:
		ModifyKnightsLeader(pUser, pkt, opcode);
		break;
	case KNIGHTS_POINT_METHOD:
		ModifyKnightsPointMethod(pUser, pkt);
		break;
	case KNIGHTS_DESTROY:
		DestroyKnights(pUser);
		break;
	case KNIGHTS_ALLLIST_REQ:
		AllKnightsList(pUser, pkt);
		break;
	case KNIGHTS_MEMBER_REQ:
		AllKnightsMember(pUser);
		break;
	case KNIGHTS_CURRENT_REQ:
		CurrentKnightsMember(pUser, pkt);
		break;
	case KNIGHTS_JOIN_REQ:
		JoinKnightsReq(pUser, pkt);
		break;
	case KNIGHTS_MARK_REGISTER:
		RegisterClanSymbol(pUser, pkt);
		break;
	case KNIGHTS_MARK_VERSION_REQ:
		RequestClanSymbolVersion(pUser, pkt);
		break;
	case KNIGHTS_MARK_REGION_REQ:
		RequestClanSymbols(pUser, pkt);
		break;
	case KNIGHTS_MARK_REQ:
		GetClanSymbol(pUser, pkt.read<uint16>());
		break;
	case KNIGHTS_ALLY_CREATE:
		KnightsAllianceCreate(pUser, pkt);
		break;
	case KNIGHTS_ALLY_REQ:
		KnightsAllianceRequest(pUser, pkt);
		break;
	case KNIGHTS_ALLY_INSERT:
		KnightsAllianceInsert(pUser, pkt);
		break;
	case KNIGHTS_ALLY_REMOVE:
		KnightsAllianceRemove(pUser, pkt);
		break;
	case KNIGHTS_ALLY_PUNISH:
		KnightsAlliancePunish(pUser, pkt);
		break;
	case KNIGHTS_ALLY_LIST:
		KnightsAllianceList(pUser, pkt);
		break;
	case KNIGHTS_TOP10:
		ListTop10Clans(pUser);
		break;
	case KNIGHTS_POINT_REQ:
		DonateNPReq(pUser, pkt);
		break;
	case KNIGHTS_DONATE_POINTS:
		DonateNP(pUser, pkt);
		break;
	case KNIGHTS_DONATION_LIST:
		DonationList(pUser, pkt);
		break;
	case KNIGHTS_NOTICE:
		UpdateClanNotice(pUser,pkt);
		break;
	case KNIGHTS_MEMO:
		UpdateKnightMemo(pUser,pkt);
		break;
	default:
		TRACE("Unhandled clan system opcode: %X\n", opcode);
	}
}

void CKnightsManager::CreateKnights(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_CREATE));
	std::string idname;
	uint8 ret_value = 0;
	pkt >> idname;

	if (idname.empty() || idname.size() > MAX_ID_SIZE
		|| !IsAvailableName(idname.c_str()))
		ret_value = 3;
	else if (pUser->GetClanID() != 0)
		ret_value = 5;
	else if (g_pMain->m_nServerGroup == 2)
		ret_value = 8;
	else if (!pUser->GetMap()->canUpdateClan())
		ret_value = 9;
	else if (pUser->GetLevel() < CLAN_LEVEL_REQUIREMENT)
		ret_value = 2;
	else if (!pUser->hasCoins(CLAN_COIN_REQUIREMENT))
		ret_value = 4;

	if (ret_value == 0)
	{
		uint16 knightindex = GetKnightsIndex(pUser->m_bNation);
		if (knightindex >= 0)
		{	
			result	<< uint8(ClanTypeTraining) 
				<< knightindex << pUser->GetNation()
				<< idname << pUser->GetName();
			g_pMain->AddDatabaseRequest(result, pUser);
			return;
		}
		ret_value = 6;
	}

	result << ret_value;
	pUser->Send(&result);
}

bool CKnightsManager::IsAvailableName( const char *strname)
{
	foreach_stlmap (itr, g_pMain->m_KnightsArray)
		if (STRCASECMP(itr->second->GetName().c_str(), strname) == 0)
			return false;

	return true;
}

int CKnightsManager::GetKnightsIndex( int nation )
{
	Guard lock(g_pMain->m_KnightsArray.m_lock);

	int knightindex = 0;
	if (nation == ELMORAD)	knightindex = 15000;

	foreach_stlmap (itr, g_pMain->m_KnightsArray)
	{
		if (itr->second != nullptr && 
			knightindex < itr->second->GetID())
		{
			if (nation == KARUS && itr->second->GetID() >= 15000)
				continue;

			knightindex = itr->second->GetID();
		}
	}

	knightindex++;
	if ((nation == KARUS && (knightindex >= 15000 || knightindex < 0))
		|| nation == ELMORAD && (knightindex < 15000 || knightindex > 30000)
		|| g_pMain->GetClanPtr(knightindex))
		return -1;

	return knightindex;
}

void CKnightsManager::JoinKnights(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;
	
	Packet result(WIZ_KNIGHTS_PROCESS);
	uint8 bResult = 0;

	do
	{
		if (!pUser->GetMap()->canUpdateClan())
			bResult = 12;
		else if (!pUser->isClanLeader() && !pUser->isClanAssistant())
			bResult = 6;

		if (bResult != 0)
			break;

		uint16 sClanID = pUser->GetClanID();
		CKnights *pKnights = g_pMain->GetClanPtr(sClanID);
		if (pKnights == nullptr)
		{
			bResult = 7;
			break;
		}

		CUser *pTUser = g_pMain->GetUserPtr(pkt.read<uint16>());
		if (pTUser == nullptr)
			bResult = 2;
		else if (pTUser->isDead())
			bResult = 3;
		else if (pTUser->GetNation() != pUser->GetNation())
			bResult = 4;
		else if (pTUser->GetClanID() > 0)
			bResult = 5;

		if (bResult != 0)
			break;

		result	<< uint8(KNIGHTS_JOIN_REQ) << uint8(1)
			<< pUser->GetSocketID() << sClanID << pKnights->m_strName;
		pTUser->Send(&result);
		return;
	} while (0);


	result << uint8(KNIGHTS_JOIN) << bResult;
	pUser->Send(&result);
}

void CKnightsManager::JoinKnightsReq(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_JOIN));
	uint8 bFlag, bResult = 0;
	uint16 sid, sClanID;
	pkt >> bFlag >> sid >> sClanID;
	CUser *pTUser = g_pMain->GetUserPtr(sid);
	if (pTUser == nullptr)
		bResult = 2;
	else if (bFlag == 0)
		bResult = 11;
	else
	{
		CKnights *pKnights = g_pMain->GetClanPtr(sClanID);
		if (pKnights == nullptr)
			bResult = 7;
		else if (pKnights->m_sMembers >= MAX_CLAN_USERS)
			bResult = 8;
	}

	if (bResult != 0)
	{
		result << bResult;
		pUser->Send(&result);
	}
	else
	{
		result << sClanID;
		g_pMain->AddDatabaseRequest(result, pUser);
	}
}

void CKnightsManager::WithdrawKnights(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS);
	uint8 bResult = 0;
	do
	{
		if (!pUser->isInClan())
			bResult = 10;
		else if (pUser->isClanLeader() && !pUser->GetMap()->canUpdateClan())
			bResult = 12;

		if (bResult != 0)
			break;

		result	<< uint8(pUser->isClanLeader() ? KNIGHTS_DESTROY : KNIGHTS_WITHDRAW)
			<< pUser->GetClanID();

		ReqKnightsAllianceRemove(pUser, pkt);
		g_pMain->AddDatabaseRequest(result, pUser);
		return;
	} while (0);

	result << bResult;
	pUser->Send(&result);
}

void CKnightsManager::DestroyKnights( CUser* pUser )
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_DESTROY));
	uint8 bResult = 1;
	if (!pUser->isClanLeader() || !pUser->isInClan())
		bResult = 0;
	else if (!pUser->GetMap()->canUpdateClan())
		bResult = 12;

	if (bResult == 1)
	{
		result << pUser->GetClanID();
		g_pMain->AddDatabaseRequest(result, pUser);
	}
	else
	{
		result << bResult;
		pUser->Send(&result);
	}
}

void CKnightsManager::ModifyKnightsLeader(CUser *pUser, Packet & pkt, uint8 opcode)
{
	if (pUser == nullptr)
		return;

	CKnights* pKnights = g_pMain->GetClanPtr(pUser->GetClanID());

	if (pKnights == nullptr)
		return;

	uint8 isClanLeader = pUser->isClanLeader() ? 1 : 2;

	Packet result(WIZ_KNIGHTS_PROCESS);

	if (opcode == KNIGHTS_HANDOVER_VICECHIEF_LIST)
	{
		uint16 ViceChiefCount = 0;

		if (g_pMain->GetUserPtr(pKnights->m_strViceChief_1, TYPE_CHARACTER))
			ViceChiefCount++;

		if (g_pMain->GetUserPtr(pKnights->m_strViceChief_2, TYPE_CHARACTER))
			ViceChiefCount++;

		if (g_pMain->GetUserPtr(pKnights->m_strViceChief_3, TYPE_CHARACTER))
			ViceChiefCount++;

		result << opcode << isClanLeader << ViceChiefCount << pKnights->m_strViceChief_1 << pKnights->m_strViceChief_2 << pKnights->m_strViceChief_3;
		pUser->Send(&result);
	}
	else if (opcode == KNIGHTS_HANDOVER_REQ) 
	{
		if (isClanLeader)
		{
			std::string strUserID;
			pkt >> strUserID;

			CUser *pTUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);

			if (pTUser == nullptr)
				return;

			pKnights->m_strChief = strUserID;

			if (pKnights->m_strViceChief_1 == strUserID) pKnights->m_strViceChief_1 = "";
			else if (pKnights->m_strViceChief_2 == strUserID) pKnights->m_strViceChief_2 = "";
			else if (pKnights->m_strViceChief_3 == strUserID) pKnights->m_strViceChief_3 = "";
			else return;
			g_DBAgent.UpdateKnights((uint8)KNIGHTS_HANDOVER, strUserID, pUser->GetClanID(), 0);

			result << (uint8)KNIGHTS_HANDOVER << pUser->GetName() << strUserID;
			pUser->Send(&result);
			pUser->ChangeFame(TRAINEE);
			pUser->UserDataSaveToAgent();
			AllKnightsMember(pUser);

			result.clear();
			result << (uint8)KNIGHTS_HANDOVER << strUserID << pUser->GetName();
			pTUser->Send(&result);
			pTUser->ChangeFame(CHIEF);
			pTUser->UserDataSaveToAgent();
			AllKnightsMember(pTUser);
		}
	}
}

void CKnightsManager::ModifyKnightsMember(CUser *pUser, Packet & pkt, uint8 opcode)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, opcode);
	uint8 bResult = 1, bRemoveFlag = 0;
	std::string strUserID;

	pkt >> strUserID;
	if (pUser->GetName() == strUserID)
		return;

	do
	{
		if (strUserID.empty() || strUserID.size() > MAX_ID_SIZE)
			bResult = 2;
		else if (!pUser->GetMap()->canUpdateClan())
			bResult = 12;
		else if (STRCASECMP(strUserID.c_str(), pUser->GetName().c_str()) == 0)
			bResult = 9;
		else if (((opcode == KNIGHTS_ADMIT || opcode == KNIGHTS_REJECT) && pUser->GetFame() < OFFICER)
			|| (opcode == KNIGHTS_PUNISH && pUser->GetFame() < VICECHIEF))
			bResult = 0;	
		else if (opcode != KNIGHTS_ADMIT && opcode != KNIGHTS_REJECT && opcode != KNIGHTS_PUNISH 
			&& !pUser->isClanLeader())
			bResult = 6;
		CKnights *pKnight = g_pMain->GetClanPtr(pUser->GetClanID());
		if (pKnight->m_strViceChief_1 == "")
			bResult = 1;	
		else if (pKnight->m_strViceChief_2 == "")
			bResult = 1;	
		else if (pKnight->m_strViceChief_3 == "")
			bResult = 1;	
		else if (opcode == KNIGHTS_VICECHIEF) bResult = 0;

		if (bResult != 1)
			break;

		CUser *pTUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
		if (pTUser == nullptr)
		{
			if (opcode != KNIGHTS_REMOVE)	
				bResult = 2;
		}
		else
		{
			if (pUser->GetNation() != pTUser->GetNation())
				bResult = 4;
			else if (pUser->GetClanID() != pTUser->GetClanID())
				bResult = 5;

			if (bResult == 1 && opcode == KNIGHTS_VICECHIEF)
			{
				if (pTUser->isClanAssistant())
					bResult = 8;
				else if (!g_pMain->GetClanPtr(pUser->GetClanID()))	
					bResult = 7;
			}

			bRemoveFlag = 1;
		}

		if (bResult != 1)
			break;

		result << pUser->GetClanID() << strUserID << bRemoveFlag;
		g_pMain->AddDatabaseRequest(result, pUser);
		return;
	} while (0);

	result << bResult;
	pUser->Send(&result);
}


void CKnightsManager::ModifyKnightsPointMethod(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr && !pUser->isClanLeader())
		return;

	CKnights *pKnights = g_pMain->GetClanPtr(pUser->GetClanID());

	if (pKnights == nullptr)
		return;

	uint8 subCode = 0;
	pkt >> subCode;

	uint8 bResult = 1;

	if (pKnights->m_byFlag >= ClanTypeAccredited5)
		pKnights->m_sClanPointMethod  = subCode != 0 ? subCode - 1 : pKnights->m_sClanPointMethod;
	else
		bResult = 2;

	g_DBAgent.UpdateKnights((uint8)KNIGHTS_POINT_METHOD, pUser->GetName(), pUser->GetClanID(), pKnights->GetClanPointMethod());

	Packet result(WIZ_KNIGHTS_PROCESS, (uint8)KNIGHTS_POINT_METHOD);
	result << bResult << pKnights->GetClanPointMethod();
	pUser->Send(&result);
}

void CKnightsManager::AllKnightsList(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLLIST_REQ));
	uint16 sPage = pkt.read<uint16>(), start = sPage * 10, count = 0;
	result << uint8(1) << sPage << count;

	foreach_stlmap (itr, g_pMain->m_KnightsArray)
	{
		CKnights* pKnights = itr->second;
		if (pKnights == nullptr
			|| !pKnights->isPromoted()
			|| pKnights->m_byNation != pUser->GetNation()
			|| count++ < start) 
			continue;

		result	<< uint16(pKnights->m_sIndex) << pKnights->m_strName 
			<< uint16(pKnights->m_sMembers) << pKnights->m_strChief 
			<< uint32(pKnights->m_nPoints);
		if (count >= start + 10)
			break;
	}

	count -= start;
	result.put(4, count);
	pUser->Send(&result);
}

void CKnightsManager::AllKnightsMember(CUser *pUser)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_MEMBER_REQ));
	uint8 bResult = 1;

	if (!pUser->isInClan())
		bResult = 2;
	else if (g_pMain->GetClanPtr(pUser->GetClanID()) == nullptr)
		bResult = 7;

	result << bResult;
	if (bResult == 1)
	{
		CKnights* pKnights = g_pMain->GetClanPtr(pUser->GetClanID());

		uint16 pktSize = 0, count = 0;
		result << pktSize << count << uint16(MAX_CLAN_USERS) << pKnights->m_strClanNotice << count;
		pktSize = (uint16)result.size();
		count = g_pMain->GetKnightsAllMembers(pUser->GetClanID(), result, pktSize, pUser->isClanLeader());
		if (count > MAX_CLAN_USERS) 
			return;

		pktSize = ((uint16)result.size() - pktSize) + 6;
		result.put(2, pktSize);
		result.put(4, count);
		result.put(10+pKnights->m_strClanNotice.size(), count);
	}
	pUser->Send(&result);
}

void CKnightsManager::CurrentKnightsMember(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_CURRENT_REQ));
	CKnights *pKnights = nullptr;
	if (!pUser->isInClan()
		|| (pKnights = g_pMain->GetClanPtr(pUser->GetClanID())) == nullptr)
	{
		result << uint8(0); // failed
		result << "is this error still used?";
		pUser->Send(&result);
		return;
	}

	uint16 page = pkt.read<uint16>();
	uint16 start = page * 10;
	uint16 count = 0;

	result	<< uint8(1)
		<< pKnights->m_strChief
		<< page;

	size_t pos = result.wpos();
	result	<< count;

	foreach_array (i, pKnights->m_arKnightsUser)
	{
		_KNIGHTS_USER *p = &pKnights->m_arKnightsUser[i];
		if (!p->byUsed || p->pSession == nullptr
			|| count++ < start)
			continue;

		CUser *pTUser = p->pSession;
		result << pUser->GetName() << pUser->GetFame() << pUser->GetLevel() << pUser->GetClass() << uint32(0);
		count++;
		if (count >= start + 10)
			break;
	}

	count -= start;
	result.put(pos, count);
	pUser->Send(&result);
}

void CKnightsManager::RecvUpdateKnights(CUser *pUser, Packet & pkt, uint8 command)
{
	if (pUser == nullptr) 
		return;

	uint16 sClanID = pkt.read<uint16>();
	CKnights *pKnights = g_pMain->GetClanPtr(sClanID);
	if (pKnights == nullptr)
		return;

	if (command == KNIGHTS_JOIN)
	{
		std::string noticeText;
		g_pMain->GetServerResource(IDS_KNIGHTS_JOIN, &noticeText, pUser->GetName().c_str());
		pKnights->AddUser(pUser);
		pKnights->SendChat("%s", noticeText.c_str());

	}
	else if (command == KNIGHTS_WITHDRAW || command == KNIGHTS_REMOVE)
	{
		pKnights->RemoveUser(pUser);
	}

	Packet result(WIZ_KNIGHTS_PROCESS, command);
	result	<< uint8(1) << pUser->GetSocketID() << pUser->GetClanID() << pUser->GetFame();

	if (command == KNIGHTS_JOIN)
	{
		result << pKnights->m_byFlag
			<< pKnights->GetAllianceID()
			<< pKnights->CapGetCapeID() 
			<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB << uint8(0)
			<< int16(pKnights->m_sMarkVersion) 
			<< pKnights->m_strName << pKnights->m_byGrade << pKnights->m_byRanking;
		pUser->AchieveMainCount(0, 0, 0, 10);
	}

	pUser->SendToRegion(&result);
}

void CKnightsManager::RecvModifyFame(CUser *pUser, Packet & pkt, uint8 command)
{
	if (pUser == nullptr) 
		return;

	std::string clanNotice;
	std::string strUserID;
	uint16 sClanID;

	pkt >> sClanID >> strUserID;

	CUser *pTUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
	CKnights *pKnights = g_pMain->GetClanPtr(sClanID);

	switch (command)
	{
	case KNIGHTS_REMOVE:
		if (pTUser != nullptr)
			pKnights->RemoveUser(pTUser);
		else
			pKnights->RemoveUser(strUserID);

		g_pMain->GetServerResource(IDS_KNIGHTS_REMOVE, &clanNotice, pTUser == nullptr ? strUserID.c_str() : pTUser->GetName().c_str());
		break;
	case KNIGHTS_ADMIT:
		if (pTUser != nullptr)
			pTUser->m_bFame = KNIGHT;
		break;
	case KNIGHTS_REJECT:
		if (pTUser != nullptr)
		{
			pTUser->SetClanID(0);
			pTUser->m_bFame = 0;

			RemoveKnightsUser(sClanID, pTUser->GetName());
		}
		break;
	case KNIGHTS_CHIEF:
		if (pTUser != nullptr)
		{
			pTUser->m_bFame = CHIEF;
			g_pMain->GetServerResource(IDS_KNIGHTS_CHIEF, &clanNotice, pTUser->GetName().c_str());
		}
		break;
	case KNIGHTS_VICECHIEF:
		if (pTUser != nullptr)
		{
			pTUser->m_bFame = VICECHIEF;
			g_pMain->GetServerResource(IDS_KNIGHTS_VICECHIEF, &clanNotice, pTUser->GetName().c_str());
		}
		break;
	case KNIGHTS_OFFICER:
		if (pTUser != nullptr)
			pTUser->m_bFame = OFFICER;
		break;
	case KNIGHTS_PUNISH:
		if (pTUser != nullptr)
			pTUser->m_bFame = PUNISH;
		break;
	}

	if (pTUser != nullptr)
		pTUser->SendClanUserStatusUpdate(command == KNIGHTS_REMOVE);

	if (clanNotice.empty())
		return;

	Packet result;
	ChatPacket::Construct(&result, KNIGHTS_CHAT, &clanNotice);

	if (command == KNIGHTS_REMOVE && pTUser != nullptr)
		pTUser->Send(&result);

	if (pKnights != nullptr)
		pKnights->Send(&result);
}

bool CKnightsManager::AddKnightsUser(int index, std::string & strUserID)
{
	CKnights *pKnights = g_pMain->GetClanPtr(index);
	return (pKnights == nullptr ? false : pKnights->AddUser(strUserID));
}

bool CKnightsManager::RemoveKnightsUser(int index, std::string & strUserID)
{
	CKnights *pKnights = g_pMain->GetClanPtr(index);
	return (pKnights == nullptr ? false : pKnights->RemoveUser(strUserID));
}

void CKnightsManager::UpdateKnightsGrade(uint16 sClanID, uint8 byFlag)
{
	CKnights * pClan = g_pMain->GetClanPtr(sClanID);
	if (pClan == nullptr)
		return;

	if (byFlag == ClanTypeTraining)
		pClan->m_sCape = -1;
	else if (byFlag == ClanTypePromoted)
		pClan->m_sCape = 0;

	pClan->m_byFlag = byFlag;
	pClan->SendUpdate();

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_UPDATE_GRADE));
	result << sClanID << byFlag << pClan->m_sCape;
	g_pMain->AddDatabaseRequest(result);
}

void CKnightsManager::UpdateClanPoint(uint16 sClanID, int32 nChangeAmount)
{
	CKnights * pClan = g_pMain->GetClanPtr(sClanID);
	if (pClan == nullptr)
		return;

	if (nChangeAmount > 0)
	{
		if (pClan->m_nClanPointFund + nChangeAmount > LOYALTY_MAX)
			pClan->m_nClanPointFund = LOYALTY_MAX;
		else
			pClan->m_nClanPointFund += nChangeAmount;
	}
	else
	{
		uint32 pChangeAmount = -nChangeAmount;

		if (pChangeAmount > pClan->m_nClanPointFund)
			pClan->m_nClanPointFund = 0;
		else
			pClan->m_nClanPointFund -= pChangeAmount;
	}

	pClan->UpdateClanFund();
}

void CKnightsManager::AddUserDonatedNP(int index, std::string & strUserID, uint32 nDonatedNP, bool bIsKillReward)
{
	CKnights *pKnights = g_pMain->GetClanPtr(index);
	if (pKnights == nullptr)
		return;

	if (bIsKillReward) {

		CUser *pUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
		if (pUser == nullptr)
			return;

		pKnights->m_nClanPointFund += nDonatedNP;
		g_DBAgent.DonateClanPoints(pUser, nDonatedNP);
	}

	for (int i = 0; i < MAX_CLAN_USERS; i++)
	{
		if (pKnights->m_arKnightsUser[i].byUsed == 0)
			continue;

		if (STRCASECMP(pKnights->m_arKnightsUser[i].strUserName.c_str(), strUserID.c_str()) == 0)
		{
			pKnights->m_arKnightsUser[i].nDonatedNP += nDonatedNP;
			break;
		}
	}
}

void CKnightsManager::RecvKnightsAllList(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLLIST_REQ));
	uint8 count = pkt.read<uint8>(), send_count = 0;
	result << send_count;
	for (int i = 0; i < count; i++)
	{
		uint32 nPoints; uint16 sClanID; uint8 bRank;
		pkt >> sClanID >> nPoints >> bRank;

		CKnights *pKnights = g_pMain->GetClanPtr(sClanID);
		if (pKnights == nullptr)
			continue;

		if (pKnights->m_nPoints != nPoints
			|| pKnights->m_byRanking != bRank)
		{
			pKnights->m_nPoints = nPoints;
			pKnights->m_byRanking = bRank;
			pKnights->m_byGrade = g_pMain->GetKnightsGrade(nPoints);

			result << sClanID << pKnights->m_byGrade << pKnights->m_byRanking;
			send_count++;
		}
	}

	result.put(1, send_count);
	g_pMain->Send_All(&result);
}

void CKnightsManager::RegisterClanSymbol(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr || !pUser->isInClan())
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_MARK_REGISTER));
	CKnights *pKnights = nullptr;
	char clanSymbol[MAX_KNIGHTS_MARK];
	uint16 sFailCode = 1, sSymbolSize = pkt.read<uint16>();

	if (!pUser->isClanLeader())
		sFailCode = 11;
	else if (pUser->GetZoneID() != pUser->GetNation())
		sFailCode = 12;
	else if (sSymbolSize == 0 
		|| sSymbolSize > MAX_KNIGHTS_MARK
		|| pkt.size() < sSymbolSize)
		sFailCode = 13;
	else if (pUser->m_iGold < CLAN_SYMBOL_COST)
		sFailCode = 14;
	else if ((pKnights = g_pMain->GetClanPtr(pUser->GetClanID())) == nullptr)
		sFailCode = 20;
	else if (!pKnights->isPromoted())
		sFailCode = 11;

	if (sFailCode != 1)
	{
		result << sFailCode;
		pUser->Send(&result);
		return;
	}

	pkt.read(clanSymbol, sSymbolSize);

	result	<< pUser->GetClanID() << sSymbolSize;
	result.append(clanSymbol, sSymbolSize);
	g_pMain->AddDatabaseRequest(result, pUser);
}

void CKnightsManager::RequestClanSymbolVersion(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr
		|| !pUser->isInClan())
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_MARK_VERSION_REQ));
	int16 sFailCode = 1;

	CKnights *pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	if (pKnights == nullptr || !pKnights->isPromoted() || !pUser->isClanLeader())
		sFailCode = 11;
	else if (pUser->GetZoneID() != pUser->GetNation())
		sFailCode = 12;

	result << sFailCode;

	if (sFailCode == 1)
		result << uint16(pKnights->m_sMarkVersion);

	pUser->Send(&result);
}

void CKnightsManager::RequestClanSymbols(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr
		|| !pUser->isInClan())
		return;

	uint16 sCount = pkt.read<uint16>();
	if (sCount > 100)
		sCount = 100;

	for (int i = 0; i < sCount; i++)
	{
		uint16 sid = pkt.read<uint16>();
		CUser *pTUser = g_pMain->GetUserPtr(sid);
		if (pTUser == nullptr
			|| !pTUser->isInGame())
			continue;

		GetClanSymbol(pTUser, pUser->GetClanID());
	}
}

void CKnightsManager::GetClanSymbol(CUser* pUser, uint16 sClanID)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS);
	CKnights *pKnights = g_pMain->GetClanPtr(sClanID);

	if (pKnights == nullptr 
			|| !pKnights->isPromoted()
			|| pKnights->m_sMarkVersion == 0
			|| pKnights->m_sMarkLen <= 0)
			return;

	result	<< uint8(KNIGHTS_MARK_REQ) << uint16(1);
	result	<< uint16(pKnights->m_byNation) << sClanID
		<< uint16(pKnights->m_sMarkVersion) << uint16(pKnights->m_sMarkLen);
	result.append(pKnights->m_Image, pKnights->m_sMarkLen);
	pUser->SendCompressed(&result);
}

bool CKnightsManager::CheckAlliance(CKnights * pMainClan, CKnights * pTargetClan)
{
	_KNIGHTS_ALLIANCE * pMainAlliance = g_pMain->GetAlliancePtr(pMainClan->GetAllianceID()) , 
					* pTargetAlliance = g_pMain->GetAlliancePtr(pTargetClan->GetAllianceID());
	if(pMainAlliance == pTargetAlliance && (pMainAlliance != nullptr || pTargetAlliance != nullptr))
		return true;
		else
			return false;
	return true;
}
void CKnightsManager::KnightsAllianceCreate(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr || pUser->isDead())
		return;
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLY_CREATE));
	CKnights * pKnights = g_pMain->GetClanPtr(pUser->m_bKnights);
	
	if(pKnights->m_byFlag > 1 && pKnights->m_strChief == pUser->GetName())
	{
		result << uint8(1) << pUser->m_bKnights;
		g_pMain->AddDatabaseRequest(result, pUser);
		pUser->Send(&result);
	}
}
void CKnightsManager::KnightsAllianceInsert(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr)
		return;
	
	Packet result(WIZ_KNIGHTS_PROCESS);
	result.SByte();
	uint8 bResult = 0;

	do
	{
		if (!pUser->isClanLeader() && !pUser->isClanAssistant())
			bResult = 6;

		if (bResult != 0)
			break;

		uint16 sClanID = pUser->GetClanID();
		CKnights *pKnights = g_pMain->GetClanPtr(sClanID);
		_KNIGHTS_ALLIANCE * pAlliance = g_pMain->GetAlliancePtr(pUser->m_bKnights);
		if (pAlliance == nullptr)
		{
			CKnights * pTknights = g_pMain->GetClanPtr(pUser->m_bKnights);
	
			if(pTknights != nullptr && pKnights->m_byFlag > 1 && pKnights->m_strChief == pUser->GetName())
			{
				result<< uint8(KNIGHTS_ALLY_CREATE) << uint8(1) << pUser->m_bKnights;
				g_pMain->AddDatabaseRequest(result, pUser);
			}
		}
		else
		{
			if (pKnights == nullptr)
			{
				bResult = 7;
				break;
			}
			
			CUser *pTUser = g_pMain->GetUserPtr(pkt.read<uint16>());
			if (pTUser == nullptr)
				bResult = 2;
			else if (pTUser->isDead())
				bResult = 3;
			else if (pTUser->GetNation() != pUser->GetNation() || pTUser->isInTempleEventZone() || pTUser->isInPKZone() || pTUser->GetMap()->isWarZone())
				bResult = 4;

			if (bResult != 0)
				break;

		
			uint16 sTUserClanID = pTUser->GetClanID();
			CKnights *pTUserKnights = g_pMain->GetClanPtr(sTUserClanID);
		
			result << uint8(KNIGHTS_ALLY_REQ);
			result << uint8(1) << pKnights->GetName();
			pTUser->Send(&result);
			return;
		}
	} 
	while (0);


	result << uint8(KNIGHTS_JOIN) << bResult;
	pUser->Send(&result);
}

void CKnightsManager::KnightsAllianceRequest(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr || pUser->isDead())
		return;

	uint8 flag;
	uint16 TargetClanLeaderID;

	pkt >> flag >> TargetClanLeaderID;

	Packet result(WIZ_KNIGHTS_PROCESS);
	uint8 bResult = 0;

	CUser * pTargetUser = g_pMain->GetUserPtr(TargetClanLeaderID);
	 if (pTargetUser == nullptr)
	 {
		 bResult = 2;
		 goto YNG;
	 }
	 else
	 {
		CKnights * pKnights = g_pMain->GetClanPtr(pTargetUser->m_bKnights);
		if(pKnights == nullptr && !pUser->isInGame() && !pTargetUser->isInGame())
		{
			bResult = 2;
			goto YNG;
		}
	
		CKnights * pTargetKnights = g_pMain->GetClanPtr(pUser->m_bKnights);
		_KNIGHTS_ALLIANCE * pAlliance = g_pMain->GetAlliancePtr(pTargetUser->m_bKnights);
		if (pTargetKnights != nullptr || pKnights != nullptr)
		{
			if (pTargetUser->isDead())
			{
				bResult = 3;
				goto YNG;
			}
			else if (pTargetUser->GetNation() != pUser->GetNation())
			{
				bResult = 4;
				goto YNG;
			}
			else if (!pTargetUser->GetClanID())
			{
				bResult = 5;
				goto YNG;
			}
			if(pAlliance != nullptr)
			{
				CKnights * pKnights1 = g_pMain->GetClanPtr(pAlliance->sSubAllianceKnights),
						 * pKnights2 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_1),  
						 * pKnights3 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_2);

				if (pKnights->m_byFlag > 1 && 
					!pTargetKnights->isInAlliance() &&
					pKnights->m_strChief == pTargetUser->GetName() &&
					pAlliance != nullptr &&
					pAlliance->sMainAllianceKnights == pKnights->GetAllianceID() &&
					pAlliance->sMercenaryClan_1 != pTargetKnights->m_sIndex &&
					pAlliance->sMercenaryClan_2 != pTargetKnights->m_sIndex &&
					pAlliance->sSubAllianceKnights != pTargetKnights->m_sIndex)
				{
					uint16 MainCap = pKnights->m_sCape;
					result << uint8(KNIGHTS_ALLY_INSERT) << uint8(1) << pTargetUser->m_bKnights << pUser->m_bKnights << MainCap;		
					g_pMain->AddDatabaseRequest(result, pTargetUser);
					std::string noticeText;
					g_pMain->GetServerResource(IDS_INSERT_KNIGHTS_ALLIANCE, &noticeText, pTargetKnights->GetName().c_str());
					pKnights->SendChatAlliance("%s", noticeText.c_str());
					if(pKnights1 != nullptr)
						pKnights1->SendChatAlliance("%s", noticeText.c_str());
					if(pKnights2 != nullptr)
						pKnights2->SendChatAlliance("%s", noticeText.c_str());
					if(pKnights3 != nullptr)
						pKnights3->SendChatAlliance("%s", noticeText.c_str());
				}
			}
		}
	}
YNG:
	{
		result << uint8(KNIGHTS_JOIN) << bResult;
		pUser->Send(&result);
	}
}

void CKnightsManager::KnightsAllianceRemove(CUser* pUser, Packet & pkt) { 
	if (pUser == nullptr || pUser->isDead())
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLY_REMOVE));
	CKnights *pKnights = g_pMain->GetClanPtr(pUser->GetClanID()),
		*pTKnights = g_pMain->GetClanPtr(pKnights->m_sAlliance);
	_KNIGHTS_ALLIANCE *pAlliance = g_pMain->GetAlliancePtr(pKnights->m_sAlliance);

	if (pAlliance == nullptr) {
		KnightsAllianceCreate(pUser,pkt);
		g_pMain->ReloadKnightAndUserRanks();
		pAlliance = g_pMain->GetAlliancePtr(pUser->GetClanID());
	} else {
 
		CKnights *pKnights1 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_1),
			*pKnights2 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_2),
			*pKnights3 = g_pMain->GetClanPtr(pAlliance->sSubAllianceKnights);

		if (pKnights->m_strChief == pUser->GetName()
			|| pTKnights->m_strChief == pUser->GetName()
			&& !pTKnights->isInAlliance()
			&& pAlliance != nullptr
			&& pAlliance->sMainAllianceKnights != pUser->GetClanID()
			|| pAlliance->sMercenaryClan_1 == pKnights->m_sIndex
			|| pAlliance->sMercenaryClan_2 == pKnights->m_sIndex
			|| pAlliance->sSubAllianceKnights == pKnights->m_sIndex) {

				result << uint8(1) << pKnights->m_sAlliance << pUser->GetClanID() << uint16(-1);
				g_pMain->AddDatabaseRequest(result, pUser);
				pKnights->m_sCape = -1;
				pUser->SendToRegion(&result);

				std::string noticeText;
				g_pMain->GetServerResource(IDS_REMOVE_KNIGHTS_ALLIANCE, &noticeText, pKnights->GetName().c_str());

				pTKnights->SendChatAlliance("%s", noticeText.c_str());
				if (pKnights1 != nullptr)
					pKnights1->SendChatAlliance("%s", noticeText.c_str());
				if (pKnights2 != nullptr)
					pKnights2->SendChatAlliance("%s", noticeText.c_str());
				if (pKnights3 != nullptr)
					pKnights3->SendChatAlliance("%s", noticeText.c_str());
		}
	}
}

void CKnightsManager::KnightsAlliancePunish(CUser* pUser, Packet & pkt) { 
	if (pUser == nullptr || pUser->isDead())
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLY_PUNISH));
	CKnights *pTKnights = g_pMain->GetClanPtr(pkt.read<uint16>()),
		*pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	_KNIGHTS_ALLIANCE *pAlliance = g_pMain->GetAlliancePtr(pUser->GetClanID());

	uint16 CapeID;

	if (pAlliance != nullptr) {
		CKnights *pKnights1 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_1),
			*pKnights2 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_2),
			*pKnights3 = g_pMain->GetClanPtr(pAlliance->sSubAllianceKnights);

		CUser *pTUser = g_pMain->GetUserPtr(pkt.read<uint16>());

		if (pTKnights != nullptr && !pTKnights->m_strChief.empty())
			pTUser = g_pMain->GetUserPtr(pTKnights->m_strChief, TYPE_CHARACTER);

		if (pAlliance->sMainAllianceKnights == pUser->GetClanID())
			CapeID = pKnights->m_sCape;
		else
			CapeID = -1;

		if (pKnights->m_byFlag > 1
			&& pKnights->m_strChief == pUser->GetName()
			&& pAlliance != nullptr
			&& pTKnights->isInAlliance()
			&& pAlliance->sMainAllianceKnights != pUser->GetClanID()
			|| pAlliance->sMercenaryClan_1 == pTKnights->m_sIndex
			|| pAlliance->sMercenaryClan_2 == pTKnights->m_sIndex
			|| pAlliance->sSubAllianceKnights == pTKnights->m_sIndex) {

				if (pTUser != nullptr) {
					result << uint8(1) << pUser->GetClanID() << pTUser->GetClanID() << CapeID;
					g_pMain->AddDatabaseRequest(result, pUser);
					pTKnights->m_sCape = CapeID;
					pTUser->SendToRegion(&result);
				} else {
					
					result << uint8(1) << pUser->GetClanID() << pkt.read<uint16>() << CapeID;
					g_pMain->AddDatabaseRequest(result, pUser);
					pTKnights->m_sCape = CapeID;
					pUser->SendToRegion(&result);
				}

				std::string noticeText;
				g_pMain->GetServerResource(IDS_PUNISH_KNIGHTS_ALLIANCE, &noticeText, pTKnights->GetName().c_str());
				pKnights->SendChatAlliance("%s", noticeText.c_str());

				if(pKnights1 != nullptr)
					pKnights1->SendChatAlliance("%s", noticeText.c_str());
				if(pKnights2 != nullptr)
					pKnights2->SendChatAlliance("%s", noticeText.c_str());
				if(pKnights3 != nullptr)
					pKnights3->SendChatAlliance("%s", noticeText.c_str());

		}
	}
}

void CKnightsManager::KnightsAllianceList(CUser* pUser, Packet & pkt)
{
	if (pUser == nullptr || !pUser->isInGame() || !pUser->isInClan())
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_ALLY_LIST));
	_KNIGHTS_ALLIANCE * pAlliance;
	CKnights * pClan = g_pMain->GetClanPtr(pUser->GetClanID());

	if (pClan == nullptr || !pClan->isInAlliance() || (pAlliance = g_pMain->GetAlliancePtr(pClan->GetAllianceID())) == nullptr)
	{
		result << uint8(0);
		pUser->Send(&result);
		return;
	}

	uint16 clans[] = 
	{ 
		pAlliance->sMainAllianceKnights, pAlliance->sSubAllianceKnights, 
		pAlliance->sMercenaryClan_1, pAlliance->sMercenaryClan_2 
	};

	size_t wpos = result.wpos();
	uint8 clanCount = 0;
	result << clanCount << std::string("XGAME");

	result.SByte();

	foreach_array(i, clans)
	{
		uint16 sClanID = clans[i]; 
		CKnights * pTmp = g_pMain->GetClanPtr(sClanID);

		if (pTmp == nullptr)
			continue;

		result << pTmp->GetID() << pTmp->GetName() << pTmp->isInAlliance() << pUser->isInGame() << pUser->GetFame() << pTmp->m_strChief;

		clanCount++;
	}

	if (clanCount == 0)
		return;

	result.put(wpos, clanCount);
	pUser->Send(&result);
}

void CKnightsManager::ListTop10Clans(CUser *pUser)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_TOP10));
	result << uint16(0);

	for (int nation = KARUS - 1; nation < ELMORAD; nation++)
	{
		for (int i = 1; i <= 5; i++)
		{
			_KNIGHTS_RATING * pRating = 
				g_pMain->m_KnightsRatingArray[nation].GetData(i);
			CKnights *pKnights = nullptr;

			if (pRating == nullptr
				|| (pKnights = g_pMain->GetClanPtr(pRating->sClanID)) == nullptr)
			{
				result	<< int16(-1)
						<< ""			
						<< int16(-1)	
						<< int16(i-1);	
			}
			else
			{
				result << pKnights->m_sIndex << pKnights->m_strName << pKnights->m_sMarkVersion << int16(i-1);
			}
		}
	}

	pUser->Send(&result);
}

void CKnightsManager::DonateNPReq(CUser * pUser, Packet & pkt)
{
	if (pUser == nullptr
		|| !pUser->isInClan())
		return;

	CKnights * pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	if (pKnights == nullptr)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_POINT_REQ));
	result	<< uint8(1) 
		<< uint32(pUser->GetLoyalty()) 
		<< uint32(pKnights->m_nClanPointFund);
	pUser->Send(&result);
}

void CKnightsManager::DonateNP(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr 
		|| !pUser->isInClan()
		|| pUser->GetLoyalty() < MIN_NP_TO_DONATE)
		return;

	CKnights * pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	if (pKnights == nullptr
		|| pKnights->m_byFlag < ClanTypeAccredited5)
		return;

	g_pMain->AddDatabaseRequest(pkt, pUser);
}

void CKnightsManager::UpdateClanNotice(CUser * pUser,Packet & pkt)
{
	if (pUser == nullptr 
		|| !pUser->isInClan())
		return;

	CKnights * pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	if (pKnights == nullptr 
		  || !pUser->isClanLeader())
		return;

	std::string Notice;

	pkt >> Notice;

	pKnights->UpdateClanNotice( Notice );
}

void CKnightsManager::DonationList(CUser *pUser, Packet & pkt)
{
	if (pUser == nullptr 
		|| !pUser->isInClan())
		return;

	CKnights * pKnights = g_pMain->GetClanPtr(pUser->GetClanID());
	if (pKnights == nullptr
		|| pKnights->m_byFlag < ClanTypeAccredited5)
		return;

	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_DONATION_LIST));
	uint8 count = 0;
	size_t wpos = result.wpos();
	result << count;

	for (int i = 0; i < MAX_CLAN_USERS; i++)
	{
		_KNIGHTS_USER * p = &pKnights->m_arKnightsUser[i];
		if (!p->byUsed)
			continue;

		result << p->strUserName << p->nDonatedNP;
		count++;
	}

	result.put(wpos, count);
	pUser->Send(&result);
}


void CKnightsManager::UpdateKnightMemo(CUser * pUser, Packet & pkt)
{
	uint8 command = pkt.read<uint8>();

	Packet result(WIZ_KNIGHTS_PROCESS);

	if(pUser == nullptr)
		return;

	CKnights* pKnights = g_pMain->GetClanPtr(pUser->GetClanID());

	if(pKnights == nullptr)
		return;

	_KNIGHTS_USER * p = nullptr;
	foreach_array (i, pKnights->m_arKnightsUser)
	{
		p = &pKnights->m_arKnightsUser[i];
		if (!p->byUsed
			|| STRCASECMP(p->strUserName.c_str(), pUser->GetName().c_str()) != 0)
			continue;

		break;
	}

	if(p == nullptr)
		return;
	
	pkt >> p->strUserMemo;

	pUser->m_strMemo = p->strUserMemo;

	result.DByte();
	result << uint8(0x58) << uint8(0x03) << uint8(0x01) <<  p->strUserMemo;

	g_pMain->Send_KnightsMember(pUser->GetClanID(),&result);
	
}