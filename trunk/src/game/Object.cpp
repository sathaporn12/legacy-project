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
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Object.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "Util.h"
#include "ObjectAccessor.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

uint32 GuidHigh2TypeId(uint32 guid_hi)
{
}

Object::Object()
{
	m_objectTypeId    = TYPEID_OBJECT;
	m_objectType      = TYPE_OBJECT;

	m_uint32Values    = 0;
	m_uint32Values_mirror = 0;
	m_valuesCount     = 0;

	m_inWorld         = false;
	m_objectUpdated   = false;

	m_PackGUID.clear();
	m_PackGUID.appendPackGUID(0);
}

Object::~Object()
{
	if(m_objectUpdated)
		ObjectAccessor::Instance().RemoveUpdateObject(this);

	if(m_uint32Values)
	{
		if(IsInWorld())
		{
			///- Do NOT call RemoveFromWorld here, if the object is a player it will crash
			sLog.outError("Object::~Object - guid="I64FMTD", typeid=%d deleted but still in world!!", GetGUID(), GetTypeId());
		}

		delete [] m_uint32Values;
		delete [] m_uint32Values_mirror;
	}
}

void Object::_InitValues()
{
	m_uint32Values = new uint32[ m_valuesCount ];
	memset(m_uint32Values, 0, m_valuesCount*sizeof(uint32));

	m_uint32Values_mirror = new uint32[ m_valuesCount ];
	memset(m_uint32Values_mirror, 0, m_valuesCount*sizeof(uint32));

	m_objectUpdated = false;
}

void Object::_Create( uint32 guidlow, HighGuid guidhigh )
{
	if(!m_uint32Values) _InitValues();

	SetUInt32Value( OBJECT_FIELD_GUID, guidlow );
	SetUInt32Value( OBJECT_FIELD_GUID+1, guidhigh);
	SetUInt32Value( OBJECT_FIELD_TYPE, m_objectType );
	m_PackGUID.clear();
	m_PackGUID.appendPackGUID(GetGUID());
}


bool Object::PrintIndexError(uint32 index, bool set) const
{
	sLog.outError("ERROR: Object::PrintIndexError");
	return false;
}

void Object::SetUInt32Value( uint16 index, uint32 value )
{
	ASSERT( index < m_valuesCount || PrintIndexError( index, true ) );

	if(m_uint32Values[ index ] != value)
	{
		m_uint32Values[ index ] = value;

		if(m_inWorld)
		{
			if(!m_objectUpdated)
			{
				ObjectAccessor::Instance().AddUpdateObject(this);
				m_objectUpdated = true;
			}
		}
	}
}

void Object::SendUpdateToPlayer(Player* player)
{
	// send update to another players
//	SendUpdateObjectToAllExcept(player);

//	sLog.outString("Object::SendUpdateToPlayer to '%s'",
//		player->GetName());

	// send create update to player
	WorldPacket packet(100);

	BuildCreateUpdateBlockForPlayer(&packet, player);
	if (0 < packet.size()) 
		player->GetSession()->SendPacket(&packet);

	// now object update/(create updated)
}

void Object::BuildCreateUpdateBlockForPlayer(WorldPacket *data, Player *target) const
{
	if(!target) {
		DEBUG_LOG("Object::BuildCreateUpdateBlockForPlayer Target is invalid");
		return;
	}

	if(target == this)     // building packet for oneself
	{
		DEBUG_LOG("Object::BuildCreateUpdateBlockForPlayer for oneself");
		((Player*)this)->BuildUpdateBlockVisibilityPacket(data);
		return;
	} 

	DEBUG_LOG("Object::BuildCreateUpdateBlockForPlayer for '%s'",
		((Player*)this)->GetName());
	((Player*)this)->BuildUpdateBlockVisibilityForOthersPacket(data);
}



WorldObject::WorldObject( WorldObject *instantiator )
{
	m_positionX    = 0;
	m_positionY    = 0;
	m_mapId        = 0;
	m_name         = "";
}

void WorldObject::_Create( uint32 guidlow, HighGuid guidhigh, uint32 mapid, uint16 x, uint16 y)
{
	Object::_Create(guidlow, guidhigh);

	m_mapId = mapid;
	m_positionX = x;
	m_positionY = y;
}