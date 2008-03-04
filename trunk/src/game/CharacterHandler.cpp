/*
 * Copyright (C) 2008-2008 LeGACY <http://www.legacy-project.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSocket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseImpl.h"
#include "Encoder.h"

// check used symbols in player name at creating and rename
std::string notAllowedChars = "\t\v\b\f\a\n\r\\\"\'\? <>[](){}_=+-|/!@#$%^&*~`.,0123456789\0";

class LoginQueryHolder : public SqlQueryHolder
{
	private:
		uint32 m_accountId;
	public:
		LoginQueryHolder(uint32 accountId)
			: m_accountId(accountId) { }
		uint32 GetAccountId() const { return m_accountId; }
		bool Initialize();
};

bool LoginQueryHolder::Initialize()
{
	sLog.outString("LoginQueryHolder::Initialize");
	SetSize(MAX_PLAYER_LOGIN_QUERY);

	bool res = true;

	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM, "SELECT * FROM characters WHERE accountid = '%u'", m_accountId);

	return res;
}



// don't call WorldSession directly
// it may get deleted before the query callbacks get executed
// instead pass an account id to this handler
class CharacterHandler
{
	public:
		void HandlePlayerLoginCallback(QueryResult * /*dummy*/, SqlQueryHolder * holder)
		{
			sLog.outString("CharacterHandler::HandlePlayerLoginCallback");
			if(!holder) return;
			WorldSession *session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
			if(!session)
			{
				sLog.outString("Session not found for %u",
					((LoginQueryHolder*)holder)->GetAccountId());
				delete holder;
				return;
			}
			session->HandlePlayerLogin((LoginQueryHolder*)holder);
		}
} chrHandler;

void WorldSession::HandleCharCreateOpcode( WorldPacket & recv_data )
{
	sLog.outString("WorldSession::HandleCharCreateOpcode");
}

void WorldSession::HandlePlayerLoginOpcode( WorldPacket & recv_data )
{
	//TODO: CHECK_PACKET_SIZE(recv_data, 8);
	
	m_playerLoading = true;

	sLog.outDebug( "WORLD: Recvd CMSG_AUTH_RESPONSE Message" );

	uint8       lenPassword;
	uint32      accountId;
	uint32      patchVer;
	std::string password;

	recv_data >> lenPassword;
	recv_data >> accountId;
	recv_data >> patchVer;
	recv_data >> password;

	LoginQueryHolder *holder = new LoginQueryHolder(accountId);
	if(!holder->Initialize())
	{
		delete holder;    // delete all unprocessed queries
		m_playerLoading = false;
		return;
	}

	sLog.outDetail("HandlePlayerLoginOpcode");

	CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerLoginCallback, holder);

}

void WorldSession::HandlePlayerLogin(LoginQueryHolder * holder)
{
	sLog.outDetail("Creating Player");
	uint32 accountId = holder->GetAccountId();

	Player* pCurrChar = new Player(this);
	//pCurrChar->GetMotionMaster()->Initialize();

	if(!pCurrChar->LoadFromDB(accountId, holder))
	{
		delete pCurrChar;  // delete it manually
		delete holder;     // delete all unprocessed queries
		m_playerLoading = false;
		return;
	}
	else
		SetPlayer(pCurrChar);

	WorldPacket data ( 5 );

	data.clear();
	data.SetOpcode(0x14); data.Prepare(); data << (uint8) 0x08;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x18); data.Prepare();
	data << (uint8) 0x05 << (uint8) 0x02 << (uint16) 0x0000;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x18); data.Prepare();
	data << (uint8) 0x05 << (uint8) 0x31 << (uint16) 0x0001;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x18); data.Prepare();
	data << (uint8) 0x05 << (uint8) 0x62 << (uint16) 0x0001;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x18); data.Prepare();
	data << (uint8) 0x05 << (uint8) 0x91 << (uint16) 0x0001;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x18); data.Prepare();
	data << (uint8) 0x05 << (uint8) 0x92 << (uint16) 0x0001;
	SendPacket(&data);

	/*
	data.clear();
	data.SetOpcode(0x14); data.Prepare();
	data << (uint8) 0x21 << (uint8) 0x00;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x23); data.Prepare();
	data << (uint32) 0x317D0C0105 << (uint32) 0x00000000 << (uint32) 0x00000000;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x24); data.Prepare();
	data << (uint8) 0x07 << (uint8) 0x03 << (uint8) 0x04;
	SendPacket(&data);

	data.clear();
	data.SetOpcode(0x26); data.Prepare();
	data << (uint8) 0x04 << (uint32) 0x00000000 << (uint32) 0x00000000;
	SendPacket(&data);
	*/

/*
	if (!MapManager::Instance().GetMap(pCurrChar->GetMapId(), pCurrChar)->AddInstanced(pCurrChar))
	{
		// TODO : Teleport to zone-in area
	}

	*/

	sLog.outDebug("Adding player %s to Map.", pCurrChar->GetName());

	MapManager::Instance().GetMap(pCurrChar->GetMapId(), pCurrChar)->Add(pCurrChar);

	pCurrChar->SendInitialPacketsAfterAddToMap();

	ObjectAccessor::Instance().AddObject(pCurrChar);

//	pCurrChar->SendInitialPacketsAfterAddToMap();

	CharacterDatabase.PExecute("UPDATE characters SET online = 1 WHERE accountid = '%u'", accountId);

	std::string IP_str = _socket ? _socket->GetRemoteAddress().c_str() : "-";
	sLog.outString("Account: %d (IP: %s) Login Character:[%s] (guid:%u)",
		GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUID());

//	pCurrChar->SetInGameTime( getMSTime() );

	sLog.outString("Map '%u' has '%u' Players", pCurrChar->GetMapId(),
		MapManager::Instance().GetMap(pCurrChar->GetMapId(), pCurrChar)->GetPlayersCount());

	m_playerLoading = false;
	delete holder;
}


void WorldSession::HandleEnterDoorOpcode( WorldPacket & recv_data )
{
	sLog.outDebug( "WORLD: Recvd CMSG_ENTER_DOOR Message" );
	uint8  sub_opcode;
	uint16 currMapId;
	uint16 doorid;

	WorldPacket data;

	GetPlayer()->SetDontMove(true);

	recv_data >> sub_opcode;
	if( sub_opcode == 0x04 )
	{
		DEBUG_LOG("Teleport ACK");
//		data.clear(); data.SetOpcode( 0x14 ); data.Prepare();
//		data << (uint8) 0x08;
//		GetPlayer()->GetSession()->SendPacket(&data);
//
		data.clear(); data.SetOpcode( 0x29 ); data.Prepare();
		data << (uint8) 0x0E;
		GetPlayer()->GetSession()->SendPacket(&data);

		///- Send teleport info, and waiting for confirmation
		currMapId = GetPlayer()->GetMapId();
		recv_data >> doorid;
		MapDoor* mapDoor = new MapDoor(currMapId, doorid);
		uint16 targetMapId = MapManager::Instance().FindMap2Map(mapDoor);
		DEBUG_LOG( "Player '%s' enter door %u in map %u will be teleported to %u", GetPlayer()->GetName(), doorid, currMapId, targetMapId);
		GetPlayer()->SetTeleportTo(targetMapId);
//		return;
	}
	else if( sub_opcode == 0x08 )
	{
		//GetPlayer()->EndOfRequest();
		//return;
	}
	

	///- Teleport Confirmed


	data.clear(); data.SetOpcode( 0x0C ); data.Prepare();
	data << (uint8) 0x96 << (uint8) 0x2E << (uint8) 0x00 << (uint8) 0x00;
	data << GetPlayer()->GetTeleportTo();
	//data << GetPlayer()->GetPositionX();
	data << (uint16) 0x007A;
	//data << GetPlayer()->GetPositionY();
	data << (uint16) 0x03A7;
	data << (uint8) 0x01;
	data << (uint8) 0x00;
	GetPlayer()->GetSession()->SendPacket(&data);

	data.clear(); data.SetOpcode( 0x05 ); data.Prepare();
	data << (uint8) 0x04;

	GetPlayer()->EndOfRequest();
//	GetPlayer()->EndOfRequest();

	GetPlayer()->SetDontMove(false);

}
