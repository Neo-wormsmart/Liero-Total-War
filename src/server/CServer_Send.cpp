/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Server class - Sending
// Created 1/7/02
// Jason Boettcher

#include <vector>

#include "LieroX.h"
#include "StringUtils.h"
#include "CServer.h"
#include "CClient.h"
#include "Protocol.h"
#include "CWorm.h"
#ifdef DEBUG
#include "MathLib.h"
#endif

using namespace std;

///////////////////
// Send a client a packet
void GameServer::SendPacket(CBytestream *bs, CClient *cl)
{
	if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
		return;

	cl->getChannel()->AddReliablePacketToSend(*bs);
}

///////////////////
// Send all the clients a packet
void GameServer::SendGlobalPacket(CBytestream *bs)
{
	// Assume reliable
	CClient *cl = cClients;

	for(short c = 0; c < MAX_CLIENTS; c++, cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;

		cl->getChannel()->AddReliablePacketToSend(*bs);
	}
}


///////////////////
// Send all the clients a string of text
void GameServer::SendGlobalText(const std::string& text, int type) {
	CBytestream bs;

	// HINT: if the message is longer than 64 characters, we split it in more messages
	// (else we could exploit old clients... :( )
	const std::vector<std::string>& split = splitstring(text, 63, iState == SVS_LOBBY ? 600 : 300, tLX->cFont);
		
	for (std::vector<std::string>::const_iterator it = split.begin(); it != split.end(); it++)  {
		// Send it
		bs.writeByte(S2C_TEXT);
		bs.writeInt(type, 1);
		bs.writeString(*it);
	}

	CClient *cl = cClients;
	for(short c = 0; c < MAX_CLIENTS; c++, cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;

		cl->getChannel()->AddReliablePacketToSend(bs);
	}
}


///////////////////
// Send a client a string of text
void GameServer::SendText(CClient *cl, const std::string& text, int type) {
	CBytestream bs;

	bs.writeByte(S2C_TEXT);
	bs.writeInt(type,1);
	bs.writeString(text);

	cl->getChannel()->AddReliablePacketToSend(bs);
}

///////////////////
// Update all the client about the playing worms
// Returns true if we sent an update
bool GameServer::SendUpdate()
{
	// Delays for different net speeds
	static const float	shootDelay[] = {0.025f, 0.010f, 0.005f, -1.0f};

	//
	// Get the update packets for each worm that needs it and save them
	//
	std::list<CBytestream> worm_bytestreams;
	std::list<CWorm *> worms_to_update;
	CWorm *w = cWorms;
	{
		int i, j;
		for (i = j = 0; j < iNumPlayers && i < MAX_WORMS; i++, w++)  {
			if (!w->isUsed())
				continue;

			++j;

			CBytestream bytes;
			// w is an own server-side copy of the worm-structure
			if (w->checkPacketNeeded()) {
				bytes.writeByte(w->getID());
				w->writePacket(&bytes);
				w->updateCheckVariables();
				worm_bytestreams.push_back(bytes);
				worms_to_update.push_back(w);
			}
		}
	}
	assert(worm_bytestreams.size() == worms_to_update.size());




	CBytestream update_packets;  // Contains all the update packets except the one from this client
	{
		int i=0;
		for (CClient *cl = cClients; i < MAX_CLIENTS; cl++, i++)  {
			if (cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
				continue;

			// Check if we have gone over the bandwidth rating for the client
			// If we have, just don't send a packet this frame
			if( !checkBandwidth(cl) )
				// We have gone over the bandwidth for the client, don't send a message this frame
				continue;

			CBytestream *bs = cl->getUnreliable();
			bs->writeByte(S2C_UPDATEWORMS);
    
			byte num_worms = 0;
   
			// Send all the _other_ worms details
			{
				update_packets.Clear();
				std::list<CWorm*>::const_iterator w_it = worms_to_update.begin();
				std::list<CBytestream>::iterator bs_it = worm_bytestreams.begin();
				for(; w_it != worms_to_update.end(); w_it++, bs_it++) {

						// Check if this client owns the worm
						if(cl->OwnsWorm(*w_it))
							continue;

						++num_worms;

						// Send out the update
						update_packets.Append(&(*bs_it));
				}
			}
    
			// Write the packets to the unreliable bytestream
			bs->writeByte(num_worms);
			bs->Append(&update_packets);
    
			// Write out a stat packet
			{
				bool need_send = false;
				{
					for (short j=0; j < cl->getNumWorms(); j++)
						if (cl->getWorm(j)->checkStatePacketNeeded())  {
							need_send = true;
							break;
						}
				}

				// Only if necessary
				if (need_send)  {
					bs->writeByte( S2C_UPDATESTATS );
					bs->writeByte( cl->getNumWorms() );
					for(short j = 0; j < cl->getNumWorms(); j++)
						cl->getWorm(j)->writeStatUpdate(bs);
				}
			}
    
    		{
				// Send the shootlist (reliable)
				CShootList *sh = cl->getShootList();
				float delay = shootDelay[cl->getNetSpeed()];
		
				if(tLX->fCurTime - sh->getStartTime() > delay && sh->getNumShots() > 0) {
					CBytestream shoot;
			
					// Send the shootlist
					if( sh->writePacket(&shoot) )
						sh->Clear();

					cl->getChannel()->AddReliablePacketToSend(shoot);
				}
			}

			SendDirtUpdate( cl );

			{
				// TODO: what is this good for?
				// Add the length of the client's unreliable packet to the frame's message size
				int *msgSize = cl->getMsgSize();
				msgSize[iServerFrame % RATE_NUMMSGS] = cl->getUnreliable()->GetLength();
			}
		}
	}

	// All good
	return true;
}


///////////////////
// Check if we have gone over the clients bandwidth rating
// Returns true if we are under the bandwidth
bool GameServer::checkBandwidth(CClient *cl)
{
	// Don't bother checking if the client is on the same comp as the server
	if( tGameInfo.iGameType != GME_LOCAL )
		return true;
	if(cl->getNetSpeed() == 3)
		return true;

	
	// Modem, ISDN, LAN, local
	// (Bytes per second)
	int	Rates[4] = {2500, 7500, 10000, 50000};


	int total = 0;
	int i;

	// Add up the message sizes
	int *msgSizes = cl->getMsgSize();
	for(i=0; i<RATE_NUMMSGS; i++)
		total += msgSizes[i];

	// Are we over the clients bandwidth rate?
	if(total > Rates[cl->getNetSpeed()]) {

		// Clear this frame size
		msgSizes[iServerFrame % RATE_NUMMSGS] = 0;

		// Don't send the packet
		return false;
	}

	// All ok
	return true;
}


///////////////////
// Send an update of the game details in the lobby
void GameServer::UpdateGameLobby(void)
{
	CBytestream bs;

	game_lobby_t *gl = &tGameLobby;

	// Check if the details have been set yet
	if(!gl->bSet)
		return;

	bs.writeByte(S2C_UPDATELOBBYGAME);
	bs.writeByte(MAX(iMaxWorms,iNumPlayers));  // This fixes the player disappearing in lobby
	bs.writeString(gl->szMapName);
    bs.writeString(gl->szModName);
    bs.writeString(gl->szModDir);
	// HACK: The VIP and CTF gametypes need to be disguised as Deathmatch or Team Deathmatches
	if(gl->nGameMode == GMT_VIP || gl->nGameMode == GMT_TEAMCTF)
		bs.writeByte(GMT_TEAMDEATH);
	else if(gl->nGameMode == GMT_CTF)
		bs.writeByte(GMT_DEATHMATCH);
	else
		bs.writeByte(gl->nGameMode);
	bs.writeInt16(gl->nLives);
	bs.writeInt16(gl->nMaxKills);
	bs.writeInt16(gl->nLoadingTime);
    bs.writeByte(gl->bBonuses);

	SendGlobalPacket(&bs);
}


///////////////////
// Send updates for all the worm lobby states
void GameServer::SendWormLobbyUpdate(void)
{
    static CBytestream bytestr;
	bytestr.Clear();
    short c,i;

    CClient *cl = cClients;
	for(c=0;c<MAX_CLIENTS;c++,cl++) {
        if( cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE )
            continue;
			
        // Set the client worms lobby ready state
        bool ready = false;
	    for(i=0; i < cl->getNumWorms(); i++) {
		    lobbyworm_t *l = cl->getWorm(i)->getLobby();
		    if(l)
			    ready = l->bReady;
	    }

	    // Let all the worms know about the new lobby state	    
		if (cl->getNumWorms() <= 2)  {
			bytestr.writeByte(S2C_UPDATELOBBY);
			bytestr.writeByte(cl->getNumWorms());
			bytestr.writeByte(ready);
			for(i=0; i<cl->getNumWorms(); i++) {
				bytestr.writeByte(cl->getWorm(i)->getID());
				bytestr.writeByte(cl->getWorm(i)->getLobby()->iTeam);
			}
		} else {
			int written = 0;
			while (written < cl->getNumWorms())  {
				bytestr.writeByte(S2C_UPDATELOBBY);
				bytestr.writeByte(1);
				bytestr.writeByte(ready);
				bytestr.writeByte(cl->getWorm(written)->getID());
				bytestr.writeByte(cl->getWorm(written)->getLobby()->iTeam);
				written++;
			}
		}
    }

	SendGlobalPacket(&bytestr);
}


///////////////////
// Tell all the clients that we're disconnecting
void GameServer::SendDisconnect(void)
{
	CClient *cl = cClients;
	if (!cl)
		return;
	
	CBytestream bs;
	bs.writeByte(S2C_LEAVING);

	for(short c=0; c<MAX_CLIENTS; c++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;

		// Send it out-of-bounds 3 times to make sure all the clients received it
		for(short i=0; i<3; i++)
			cl->getChannel()->Transmit(&bs);
	}	
}


///////////////////
// Update the worm names, skins, colours etc
void GameServer::UpdateWorms(void)
{
	CBytestream bytestr;
	CWorm* w = cWorms;
	for(int i = 0; i < MAX_WORMS; i++, w++) {
		if(!w->isUsed())
			continue;
		bytestr.writeByte(S2C_WORMINFO);
		bytestr.writeInt(w->getID(), 1);
		w->writeInfo(&bytestr);
	}
	SendGlobalPacket(&bytestr);
}

#ifdef DEBUG
///////////////
// Used for testing network stability
void GameServer::SendRandomPacket()
{
	CBytestream bs;
	int random_length = GetRandomInt(50);
	for (int i=0; i < random_length; i++)
		bs.writeByte((uchar)GetRandomInt(255));

	SendGlobalPacket(&bs);
	printf("A random packet has been sent to all clients\n");
}
#endif

void GameServer::SendDirtUpdate( CClient * cl )
{
	if( ! tLXOptions->bServerSendsDirtUpdates || 
		cl->getClientOLXVer() < 4 ||
		cl == & cClients[0] )	// Do not update dirt for local client
		return;

	if( cl->getFileDownloaderInGame()->getState() != CFileDownloaderInGame::S_SEND )
	{
		// Send dirt update once in 7 seconds (I believe that's not too often)
		if( cl->getLastDirtUpdate() + 7.0f > tLX->fCurTime )
			return;
		if( cl->getPreviousDirtMap()->GetLength() == 0 )
		{
			cl->setPartialDirtUpdateCount(0);
			cl->getPreviousDirtMap()->Clear();
			cMap->SendDirtUpdate(cl->getPreviousDirtMap());
			cl->getFileDownloaderInGame()->setDataToSend( "dirt:", cl->getPreviousDirtMap()->readData() );
		}
		else
		{
			CBytestream bs, bsDiff;
			cMap->SendDirtUpdate(&bs);
			cl->getPreviousDirtMap()->ResetPosToBegin();
			while( !bs.isPosAtEnd() )
				bsDiff.writeBit( bs.readBit() != cl->getPreviousDirtMap()->readBit() );
			* cl->getPreviousDirtMap() = bs;
			cl->getFileDownloaderInGame()->setDataToSend( "dirt:" + itoa(cl->getPartialDirtUpdateCount()), bsDiff.readData() );
			cl->setPartialDirtUpdateCount( cl->getPartialDirtUpdateCount() + 1 );
		};
	};
	if( cl->getFileDownloaderInGame()->getState() == CFileDownloaderInGame::S_SEND )
	{
		// If client is laggy send packets very rarely
		if( cl->getLastDirtUpdate() + cl->getPing()*2.5f/1000.0f > tLX->fCurTime )
			return;	
		CBytestream bs;
		bs.writeByte( S2C_SENDFILE );
		cl->getFileDownloaderInGame()->send( &bs );
		SendPacket( &bs, cl );
		cl->setLastDirtUpdate( tLX->fCurTime );
	};
}

void GameServer::SendFiles()
{
	
	if(iState != SVS_LOBBY)
		return;

	// Server will keep min rate of 1 kbyte/second - send each second 4 packets of 256 bytes each.
	// If client sends ping packets server will send packets faster.
	const float MaxPacketDelay = 0.25; // Modify this to get higher packet rate, though it may overflood laggy clients

	bool startTimer = false;
	
	CClient *cl = cClients;

	for(int c = 0; c < MAX_CLIENTS; c++, cl++) 
	{
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;

		if( cl->getFileDownloaderInGame()->getState() == CFileDownloaderInGame::S_SEND &&
			cl->getLastFileRequestPacketReceived() + MaxPacketDelay <= tLX->fCurTime )
		{
			startTimer = true;
			cl->setLastFileRequestPacketReceived( tLX->fCurTime );
			CBytestream bs;
			bs.writeByte(S2C_SENDFILE);
			cl->getFileDownloaderInGame()->send(&bs);
			SendPacket( &bs, cl );
		};
	};

	if( startTimer )
		Timer( &Timer::DummyHandler, NULL, int(MaxPacketDelay*1000.0), true ).startHeadless();
};

void GameServer::sendEmptyWeaponsOnRespawn( CWorm * Worm )
{
	CBytestream bs;
	CClient * cl = Worm->getClient();
	int i, j, curWeapon = Worm->getCurrentWeapon();
	for( i = 0; i < 5; i++ )
	{
		Worm->getWeapon(i)->Charge=0;
		Worm->getWeapon(i)->Reloading=1;
	};
	for( i=0; i<5; i++ )
	{
		if( i != curWeapon )
		{
			Worm->setCurrentWeapon(i);
			bs.writeByte( S2C_UPDATESTATS );
			bs.writeByte( cl->getNumWorms() );
			for( j = 0; j < cl->getNumWorms(); j++ )
				cl->getWorm(j)->writeStatUpdate(&bs);
		};
	};
	Worm->setCurrentWeapon(curWeapon);
	bs.writeByte( S2C_UPDATESTATS );
	bs.writeByte( cl->getNumWorms() );
	for( j = 0; j < cl->getNumWorms(); j++ )
		cl->getWorm(j)->writeStatUpdate(&bs);
	SendPacket(&bs, cl);
};
