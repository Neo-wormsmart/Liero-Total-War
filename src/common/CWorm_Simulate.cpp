/////////////////////////////////////////
//
//             OpenLieroX
//
//    Worm class - input handling
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// TODO: rename this file (only input handling here)

// Created 2/8/02
// Jason Boettcher



#include "LieroX.h"
#include "Sounds.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "CWorm.h"
#include "MathLib.h"
#include "Entity.h"
#include "CClient.h"
#include "CServerConnection.h"
#include "CWormHuman.h"
#include "ProfileSystem.h"
#include "CGameScript.h"
#include "Debug.h"
#include "CGameMode.h"




///////////////////
// Get the input from a human worm
void CWormHumanInputHandler::getInput() {		
	// HINT: we are calling this from simulateWorm
	TimeDiff dt = tLX->currentTime - m_worm->fLastInputTime;
	m_worm->fLastInputTime = tLX->currentTime;

	CVec	dir;
	int		weap = false;

	mouse_t *ms = GetMouse();

	// do it here to ensure that it is called exactly once in a frame (needed because of intern handling)
	bool leftOnce = cLeft.isDownOnce();
	bool rightOnce = cRight.isDownOnce();

	worm_state_t *ws = &m_worm->tState;

	// Init the ws
	ws->bCarve = false;
	ws->bMove = false;
	ws->bShoot = false;
	ws->bJump = false;

	const bool mouseControl = 
			tLXOptions->bMouseAiming &&
			( cClient->isHostAllowingMouse() || tLX->iGameType == GME_LOCAL) &&
			ApplicationHasFocus(); // if app has no focus, don't use mouseaiming, the mousewarping is pretty annoying then
	const float mouseSensity = (float)tLXOptions->iMouseSensity; // how sensitive is the mouse in X/Y-dir

	// TODO: here are width/height of the window hardcoded
	int mouse_dx = ms->X - 640/2;
	int mouse_dy = ms->Y - 480/2;
	if(mouseControl) SDL_WarpMouse(640/2, 480/2);

	{
/*		// only some debug output for checking the values
		if(mouseControl && (mouse_dx != 0 || mouse_dy != 0))
			notes("mousepos changed: %i, %i\n", mouse_dx, mouse_dy),
			notes("anglespeed: %f\n", fAngleSpeed),
			notes("movespeed: %f\n", fMoveSpeedX),
			notes("dt: %f\n", dt); */
	}

	// angle section
	{
		static const float joystickCoeff = 150.0f/65536.0f;
		static const float joystickShift = 15; // 15 degrees

		// Joystick up
		if (cDown.isJoystickThrottle())  {
			m_worm->fAngleSpeed = 0;
			m_worm->fAngle = CLAMP((float)cUp.getJoystickValue() * joystickCoeff - joystickShift, -90.0f, 60.0f);
		}

		// Joystick down
		if (cDown.isJoystickThrottle())  {
			m_worm->fAngleSpeed = 0;
			m_worm->fAngle = CLAMP((float)cUp.getJoystickValue() * joystickCoeff - joystickShift, -90.0f, 60.0f);
		}

		// Up
		if(cUp.isDown() && !cUp.isJoystickThrottle()) {
			// HINT: 500 is the original value here (rev 1)
			m_worm->fAngleSpeed -= 500 * dt.seconds();
		} else if(cDown.isDown() && !cDown.isJoystickThrottle()) { // Down
			// HINT: 500 is the original value here (rev 1)
			m_worm->fAngleSpeed += 500 * dt.seconds();
		} else {
			if(!mouseControl) {
				// HINT: this is the original order and code (before mouse patch - rev 1007)
				CLAMP_DIRECT(m_worm->fAngleSpeed, -100.0f, 100.0f);
				REDUCE_CONST(m_worm->fAngleSpeed, 200*dt.seconds());
				RESET_SMALL(m_worm->fAngleSpeed, 5.0f);

			} else { // mouseControl for angle
				// HINT: to behave more like keyboard, we should use CLAMP(..500) here
				float diff = mouse_dy * mouseSensity;
				CLAMP_DIRECT(diff, -500.0f, 500.0f); // same limit as keyboard
				m_worm->fAngleSpeed += diff * dt.seconds();

				// this tries to be like keyboard where this code is only applied if up/down is not pressed
				if(abs(mouse_dy) < 5) {
					CLAMP_DIRECT(m_worm->fAngleSpeed, -100.0f, 100.0f);
					REDUCE_CONST(m_worm->fAngleSpeed, 200*dt.seconds());
					RESET_SMALL(m_worm->fAngleSpeed, 5.0f);
				}
			}
		}

		m_worm->fAngle += m_worm->fAngleSpeed * dt.seconds();
		if(CLAMP_DIRECT(m_worm->fAngle, -90.0f, 60.0f) != 0)
			m_worm->fAngleSpeed = 0;

		// Calculate dir
		dir.x=( (float)cos(m_worm->fAngle * (PI/180)) );
		dir.y=( (float)sin(m_worm->fAngle * (PI/180)) );
		if( m_worm->iMoveDirection == DIR_LEFT ) // Fix: Ninja rope shoots backwards when you strafing or mouse-aiming
			dir.x=(-dir.x);

	} // end angle section


	// basic mouse control (moving)
	if(mouseControl) {
		// no dt here, it's like the keyboard; and the value will be limited by dt later
		m_worm->fMoveSpeedX += mouse_dx * mouseSensity * 0.01f;

		REDUCE_CONST(m_worm->fMoveSpeedX, 1000*dt.seconds());
		//RESET_SMALL(m_worm->fMoveSpeedX, 5.0f);
		CLAMP_DIRECT(m_worm->fMoveSpeedX, -500.0f, 500.0f);

		if(fabs(m_worm->fMoveSpeedX) > 50) {
			if(m_worm->fMoveSpeedX > 0) {
				m_worm->iMoveDirection = DIR_RIGHT;
				if(mouse_dx < 0) m_worm->lastMoveTime = tLX->currentTime;
			} else {
				m_worm->iMoveDirection = DIR_LEFT;
				if(mouse_dx > 0) m_worm->lastMoveTime = tLX->currentTime;
			}
			ws->bMove = true;
			if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown())
				m_worm->iDirection = m_worm->iMoveDirection;

		} else {
			ws->bMove = false;
		}

	}


	if(mouseControl) { // set shooting, ninja and jumping, weapon selection for mousecontrol
		// like Jason did it
		ws->bShoot = (ms->Down & SDL_BUTTON(1)) ? true : false;
		ws->bJump = (ms->Down & SDL_BUTTON(3)) ? true : false;
		if(ws->bJump) {
			if(m_worm->cNinjaRope.isReleased())
				m_worm->cNinjaRope.Release();
		}
		else if(ms->FirstDown & SDL_BUTTON(2)) {
			// TODO: this is bad. why isn't there a ws->iNinjaShoot ?
			m_worm->cNinjaRope.Shoot(m_worm->vPos, dir);
			PlaySoundSample(sfxGame.smpNinja);
		}

		if( ms->WheelScrollUp || ms->WheelScrollDown ) {
			m_worm->bForceWeapon_Name = true;
			m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
			if( ms->WheelScrollUp )
				m_worm->iCurrentWeapon ++;
			else
				m_worm->iCurrentWeapon --;
			if(m_worm->iCurrentWeapon >= m_worm->iNumWeaponSlots)
				m_worm->iCurrentWeapon=0;
			if(m_worm->iCurrentWeapon < 0)
				m_worm->iCurrentWeapon=m_worm->iNumWeaponSlots-1;
		}
	}



	{ // set carving

/*		// this is a bit unfair to keyboard players
		if(mouseControl) { // mouseControl
			if(fabs(fMoveSpeedX) > 200) {
				ws->iCarve = true;
			}
		} */

		const float carveDelay = 0.2f;

		if((mouseControl && ws->bMove && m_worm->iMoveDirection == DIR_LEFT)
			|| ((( cLeft.isJoystick() && cLeft.isDown()) || (cLeft.isKeyboard() && leftOnce)) && !cSelWeapon.isDown())) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if((mouseControl && ws->bMove && m_worm->iMoveDirection == DIR_RIGHT)
			|| ((( cRight.isJoystick() && cRight.isDown()) || (cRight.isKeyboard() && rightOnce)) && !cSelWeapon.isDown())) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}
	}

    //
    // Weapon changing
	//
	if(cSelWeapon.isDown()) {
		// TODO: was is the intention of this var? if weapon change, then it's wrong
		// if cSelWeapon.isDown(), then we don't need it
		weap = true;

		// we don't want keyrepeats here, so only count the first down-event
		int change = (rightOnce ? 1 : 0) - (leftOnce ? 1 : 0);
		m_worm->iCurrentWeapon += change;
		MOD(m_worm->iCurrentWeapon, m_worm->iNumWeaponSlots);

		// Joystick: if the button is pressed, change the weapon (it is annoying to move the axis for weapon changing)
		if (cSelWeapon.isJoystick() && change == 0 && cSelWeapon.isDownOnce())  {
			m_worm->iCurrentWeapon++;
			MOD(m_worm->iCurrentWeapon, m_worm->iNumWeaponSlots);
		}
	}

	// Process weapon quick-selection keys
	for(size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]); i++ )
	{
		if( cWeapons[i].isDown() )
		{
			m_worm->iCurrentWeapon = i;
			// Let the weapon name show up for a short moment
			m_worm->bForceWeapon_Name = true;
			m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
		}
	}


	// Safety: clamp the current weapon
	m_worm->iCurrentWeapon = CLAMP(m_worm->iCurrentWeapon, 0, m_worm->iNumWeaponSlots-1);


	ws->bShoot = cShoot.isDown();

	if(!cSelWeapon.isDown()) {
		if(cLeft.isDown()) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!cRight.isDown()) {
				if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) m_worm->iDirection = DIR_LEFT;
				m_worm->iMoveDirection = DIR_LEFT;
			}

			if(rightOnce) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if(cRight.isDown()) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!cLeft.isDown()) {
				if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) m_worm->iDirection = DIR_RIGHT;
				m_worm->iMoveDirection = DIR_RIGHT;
			}

			if(leftOnce) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		// inform player about disallowed strafing
		if(!cClient->isHostAllowingStrafing() && cStrafe.isDownOnce())
			// TODO: perhaps in chat?
			hints << "strafing is not allowed on this server." << endl;
	}


	bool oldskool = tLXOptions->bOldSkoolRope;

	bool jumpdownonce = cJump.isDownOnce();

	// Jump
	if(jumpdownonce) {
		if( !(oldskool && cSelWeapon.isDown()) )  {
			ws->bJump = true;

			if(m_worm->cNinjaRope.isReleased())
				m_worm->cNinjaRope.Release();
		}
	}

	// Ninja Rope
	if(oldskool) {
		// Old skool style rope throwing
		// Change-weapon & jump

		if(!cSelWeapon.isDown() || !cJump.isDown())  {
			m_worm->bRopeDown = false;
		}

		if(cSelWeapon.isDown() && cJump.isDown() && !m_worm->bRopeDown) {

			m_worm->bRopeDownOnce = true;
			m_worm->bRopeDown = true;
		}

		// Down
		if(m_worm->bRopeDownOnce) {
			m_worm->bRopeDownOnce = false;

			m_worm->cNinjaRope.Shoot(m_worm->vPos,dir);

			// Throw sound
			PlaySoundSample(sfxGame.smpNinja);
		}


	} else {
		// Newer style rope throwing
		// Seperate dedicated button for throwing the rope
		if(cInpRope.isDownOnce()) {

			m_worm->cNinjaRope.Shoot(m_worm->vPos,dir);
			// Throw sound
			PlaySoundSample(sfxGame.smpNinja);
		}
	}

	ws->iAngle = (int)m_worm->fAngle;
	ws->iX = (int)m_worm->vPos.x;
	ws->iY = (int)m_worm->vPos.y;


	cUp.reset();
	cDown.reset();
	cLeft.reset();
	cRight.reset();
	cShoot.reset();
	cJump.reset();
	cSelWeapon.reset();
	cInpRope.reset();
	cStrafe.reset();
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
}


void CWorm::NewNet_GetInput( NewNet::KeyState_t keys, NewNet::KeyState_t keysChanged ) // Synthetic input from new net engine - Ignores inputHandler
{
	CVec	dir;
	TimeDiff dt ( (int)NewNet::TICK_TIME );
	
	// do it here to ensure that it is called exactly once in a frame (needed because of intern handling)
	bool leftOnce = keys.keys[NewNet::K_LEFT] && keysChanged.keys[NewNet::K_LEFT];
	bool rightOnce = keys.keys[NewNet::K_RIGHT] && keysChanged.keys[NewNet::K_RIGHT];
	
	//if( NewNet::CanUpdateGameState() )
	//printf("keys.keys[NewNet::K_LEFT] %i leftOnce %i time %i canUpdate %i\n", keys.keys[NewNet::K_LEFT], leftOnce, (int)NewNet::GetCurTime().milliseconds(), NewNet::CanUpdateGameState() );
	
	worm_state_t *ws = &tState;

	// Init the ws
	ws->bCarve = false;
	ws->bMove = false;
	ws->bShoot = false;
	ws->bJump = false;

	{
		// Up
		if(keys.keys[NewNet::K_UP]) {
			// HINT: 500 is the original value here (rev 1)
			fAngleSpeed -= 500 * dt.seconds();
		} else if(keys.keys[NewNet::K_DOWN]) { // Down
			// HINT: 500 is the original value here (rev 1)
			fAngleSpeed += 500 * dt.seconds();
		} else {
				// HINT: this is the original order and code (before mouse patch - rev 1007)
				CLAMP_DIRECT(fAngleSpeed, -100.0f, 100.0f);
				REDUCE_CONST(fAngleSpeed, 200*dt.seconds());
				RESET_SMALL(fAngleSpeed, 5.0f);

		}

		fAngle += fAngleSpeed * dt.seconds();
		if(CLAMP_DIRECT(fAngle, -90.0f, 60.0f) != 0)
			fAngleSpeed = 0;

		// Calculate dir
		dir.x=( (float)cos(fAngle * (PI/180)) );
		dir.y=( (float)sin(fAngle * (PI/180)) );
		if( iMoveDirection == DIR_LEFT ) // Fix: Ninja rope shoots backwards when you strafing or mouse-aiming
			dir.x=(-dir.x);

	} // end angle section

	{ // set carving

		const float carveDelay = 0.2f;

		if(leftOnce && !keys.keys[NewNet::K_SELWEAP]) {

			if(NewNet::GetCurTime() - fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				fLastCarve = NewNet::GetCurTime();
			}
		}

		if(rightOnce && !keys.keys[NewNet::K_SELWEAP]) {

			if(NewNet::GetCurTime() - fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				fLastCarve = NewNet::GetCurTime();
			}
		}
	}

    //
    // Weapon changing
	//
	if(keys.keys[NewNet::K_SELWEAP]) {
		// we don't want keyrepeats here, so only count the first down-event
		int change = (rightOnce ? 1 : 0) - (leftOnce ? 1 : 0);
		iCurrentWeapon += change;
		MOD(iCurrentWeapon, iNumWeaponSlots);
	}

	// Safety: clamp the current weapon
	iCurrentWeapon = CLAMP(iCurrentWeapon, 0, iNumWeaponSlots-1);

	ws->bShoot = keys.keys[NewNet::K_SHOOT];

	if(!keys.keys[NewNet::K_SELWEAP]) {
		if(keys.keys[NewNet::K_LEFT]) {
			ws->bMove = true;
			lastMoveTime = NewNet::GetCurTime();

			if(!keys.keys[NewNet::K_RIGHT]) {
				//if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) iDirection = DIR_LEFT;
				iDirection = DIR_LEFT;
				iMoveDirection = DIR_LEFT;
			}

			if(rightOnce) {
				ws->bCarve = true;
				fLastCarve = NewNet::GetCurTime();
			}
		}

		if(keys.keys[NewNet::K_RIGHT]) {
			ws->bMove = true;
			lastMoveTime = NewNet::GetCurTime();

			if(!keys.keys[NewNet::K_LEFT]) {
				//if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) iDirection = DIR_RIGHT;
				iDirection = DIR_RIGHT;
				iMoveDirection = DIR_RIGHT;
			}

			if(leftOnce) {
				ws->bCarve = true;
				fLastCarve = NewNet::GetCurTime();
			}
		}
	}


	bool jumpdownonce = keys.keys[NewNet::K_JUMP] && keysChanged.keys[NewNet::K_JUMP];

	// Jump
	if(jumpdownonce) {
			ws->bJump = true;

			if(cNinjaRope.isReleased())
				cNinjaRope.Release();
	}

	// Newer style rope throwing
	// Seperate dedicated button for throwing the rope
	if( keys.keys[NewNet::K_ROPE] && keysChanged.keys[NewNet::K_ROPE] ) {
		cNinjaRope.Shoot(vPos,dir);
		// Throw sound
		PlaySoundSample(sfxGame.smpNinja);
	}

	ws->iAngle = (int)fAngle;
	ws->iX = (int)vPos.x;
	ws->iY = (int)vPos.y;
}


///////////////////
// Clear the input
void CWormHumanInputHandler::clearInput() {
	// clear inputs
	cUp.reset();
	cDown.reset();
	cLeft.reset();
	cRight.reset();
	cShoot.reset();
	cJump.reset();
	cSelWeapon.reset();
	cInpRope.reset();
	cStrafe.reset();
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
}



struct HumanWormType : WormType {
	virtual CWormInputHandler* createInputHandler(CWorm* w) { return new CWormHumanInputHandler(w); }
	int toInt() { return 0; }
} PRF_HUMAN_instance;
WormType* PRF_HUMAN = &PRF_HUMAN_instance;

CWormHumanInputHandler::CWormHumanInputHandler(CWorm* w) : CWormInputHandler(w) {	
	// we use the normal init system first after the weapons are selected and we are ready
	stopInputSystem();
}

CWormHumanInputHandler::~CWormHumanInputHandler() {}

void CWormHumanInputHandler::startGame() {
	initInputSystem();
}



///////////////////
// Setup the inputs
void CWormHumanInputHandler::setupInputs(const controls_t& Inputs)
{
	//bUsesMouse = false;
	for (byte i=0;i<Inputs.ControlCount(); i++)
		if (Inputs[i].find("ms"))  {
			//bUsesMouse = true;
			break;
		}

	cUp.Setup(		Inputs[SIN_UP] );
	cDown.Setup(	Inputs[SIN_DOWN] );
	cLeft.Setup(	Inputs[SIN_LEFT] );
	cRight.Setup(	Inputs[SIN_RIGHT] );

	cShoot.Setup(	Inputs[SIN_SHOOT] );
	cJump.Setup(	Inputs[SIN_JUMP] );
	cSelWeapon.Setup(Inputs[SIN_SELWEAP] );
	cInpRope.Setup(	Inputs[SIN_ROPE] );

	cStrafe.Setup( Inputs[SIN_STRAFE] );

	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].Setup(Inputs[SIN_WEAPON1 + i]);
}


void CWormHumanInputHandler::initInputSystem() {
	cUp.setResetEachFrame( false );
	cDown.setResetEachFrame( false );
	cLeft.setResetEachFrame( false );
	cRight.setResetEachFrame( false );
	cShoot.setResetEachFrame( false );
	cJump.setResetEachFrame( false );
	cSelWeapon.setResetEachFrame( false );
	cInpRope.setResetEachFrame( false );
	cStrafe.setResetEachFrame( false );
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].setResetEachFrame( false );
}

void CWormHumanInputHandler::stopInputSystem() {
	cUp.setResetEachFrame( true );
	cDown.setResetEachFrame( true );
	cLeft.setResetEachFrame( true );
	cRight.setResetEachFrame( true );
	cShoot.setResetEachFrame( true );
	cJump.setResetEachFrame( true );
	cSelWeapon.setResetEachFrame( true );
	cInpRope.setResetEachFrame( true );
	cStrafe.setResetEachFrame( true );
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].setResetEachFrame( true );
}





///////////////////
// Initialize the weapon selection screen
void CWormHumanInputHandler::initWeaponSelection() {
	// This is used for the menu screen as well
	m_worm->iCurrentWeapon = 0;
	
	m_worm->bWeaponsReady = false;
	
	m_worm->iNumWeaponSlots = 5;
	
	m_worm->clearInput();
	
	// Safety
	if (!m_worm->tProfile)  {
		errors << "initWeaponSelection called and tProfile is not set" << endl;
		return;
	}
	
	// Load previous settings from profile
	short i;
	for(i=0;i<m_worm->iNumWeaponSlots;i++) {
		
		m_worm->tWeapons[i].Weapon = m_worm->cGameScript->FindWeapon( m_worm->tProfile->sWeaponSlots[i] );
		
        // If this weapon is not enabled in the restrictions, find another weapon that is enabled
        if( !m_worm->cWeaponRest->isEnabled( m_worm->tWeapons[i].Weapon->Name ) || !m_worm->cGameScript->weaponExists( m_worm->tWeapons[i].Weapon->Name ) ) {
			
            m_worm->tWeapons[i].Weapon = m_worm->cGameScript->FindWeapon( m_worm->cWeaponRest->findEnabledWeapon( m_worm->cGameScript ) );
        }
	}
	
	
	for(short n=0;n<m_worm->iNumWeaponSlots;n++) {
		m_worm->tWeapons[n].Charge = 1;
		m_worm->tWeapons[n].Reloading = false;
		m_worm->tWeapons[n].SlotNum = n;
		m_worm->tWeapons[n].LastFire = 0;
	}
	// Skip weapon selection dialog if we're spectating
	if( cClient->getSpectate() )
		m_worm->bWeaponsReady = true;
	
	// Skip the dialog if there's only one weapon available
	int enabledWeaponsAmount = 0;
	for( int f = 0; f < m_worm->cGameScript->GetNumWeapons(); f++ )
		if( m_worm->cWeaponRest->isEnabled( m_worm->cGameScript->GetWeapons()[f].Name ) )
			enabledWeaponsAmount++;
	
	if( enabledWeaponsAmount <= 1 ) // server can ban ALL weapons, noone will be able to shoot then
		m_worm->bWeaponsReady = true;

	if(!m_worm->bWeaponsReady && cClient->getGameLobby()->gameMode == GameMode(GM_HIDEANDSEEK)) {
		// just skip weapon selection in HideAndSeek games
		m_worm->bWeaponsReady = true;
	}
}


///////////////////
// Draw/Process the weapon selection screen
void CWormHumanInputHandler::doWeaponSelectionFrame(SDL_Surface * bmpDest, CViewport *v)
{
	// TODO: this should also be used for selecting the weapons for the bot (but this in CWorm_AI then)
	// TODO: reduce local variables in this function
	// TODO: make this function shorter
	// TODO: give better names to local variables
		
	if(bDedicated) {
		warnings << "doWeaponSelectionFrame: we have a local human input in our dedicated server" << endl; 
		return; // just for safty; atm this function only handles non-bot players
	}
	
	int l = 0;
	int t = 0;
	short i;
	int centrex = 320; // TODO: hardcoded screen width here
	
    if( v ) {
        if( v->getUsed() ) {
            l = v->GetLeft();
	        t = v->GetTop();
            centrex = v->GetLeft() + v->GetVirtW()/2;
        }
    }
	
	tLX->cFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "~ Weapons Selection ~");
		
	tLX->cFont.DrawCentre(bmpDest, centrex, t+48, tLX->clWeaponSelectionTitle, "(Use up/down and left/right for selection.)");
	tLX->cFont.DrawCentre(bmpDest, centrex, t+66, tLX->clWeaponSelectionTitle, "(Go to 'Done' and press shoot then.)");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	
	bool bChat_Typing = cClient->isTyping();
	
	int y = t + 100;
	for(i=0;i<m_worm->iNumWeaponSlots;i++) {
		
		//tLX->cFont.Draw(bmpDest, centrex-69, y+1, 0,"%s", tWeapons[i].Weapon->Name.c_str());
		if(m_worm->iCurrentWeapon == i)
			tLX->cOutlineFont.Draw(bmpDest, centrex-70, y, tLX->clWeaponSelectionActive,  m_worm->tWeapons[i].Weapon->Name);
		else
			tLX->cOutlineFont.Draw(bmpDest, centrex-70, y, tLX->clWeaponSelectionDefault,  m_worm->tWeapons[i].Weapon->Name);
		
		if (bChat_Typing)  {
			y += 18;
			continue;
		}
		
		// Changing weapon
		if(m_worm->iCurrentWeapon == i && !bChat_Typing) {
			int change = cRight.wasDown() - cLeft.wasDown();
			if(cSelWeapon.isDown()) change *= 6; // jump with multiple speed if selWeapon is pressed
			int id = m_worm->tWeapons[i].Weapon->ID;
			if(change > 0) while(change) {
				id++; MOD(id, m_worm->cGameScript->GetNumWeapons());
				if( m_worm->cWeaponRest->isEnabled( m_worm->cGameScript->GetWeapons()[id].Name ) )
					change--;
				if(id == m_worm->tWeapons[i].Weapon->ID) // back where we were before
					break;
			} else
				if(change < 0) while(change) {
					id--; MOD(id, m_worm->cGameScript->GetNumWeapons());
					if( m_worm->cWeaponRest->isEnabled( m_worm->cGameScript->GetWeapons()[id].Name ) )
						change++;
					if(id == m_worm->tWeapons[i].Weapon->ID) // back where we were before
						break;
				}
			m_worm->tWeapons[i].Weapon = &m_worm->cGameScript->GetWeapons()[id];
		}
		
		y += 18;
	}
	
	for(i=0;i<5;i++)
		m_worm->tProfile->sWeaponSlots[i] = m_worm->tWeapons[i].Weapon->Name;
	
    // Note: The extra weapon weapon is the 'random' button
    if(m_worm->iCurrentWeapon == m_worm->iNumWeaponSlots) {
		
		// Fire on the random button?
		if((cShoot.isDownOnce()) && !bChat_Typing) {
			m_worm->GetRandomWeapons();
		}
	}
	
	
	// Note: The extra weapon slot is the 'done' button
	if(m_worm->iCurrentWeapon == m_worm->iNumWeaponSlots+1) {
		
		// Fire on the done button?
		// we have to check isUp() here because if we continue while it is still down, we will fire after in the game
		if((cShoot.isUp()) && !bChat_Typing) {
			// we are ready with manual human weapon selection
			m_worm->bWeaponsReady = true;
			m_worm->iCurrentWeapon = 0;
			
			// Set our profile to the weapons (so we can save it later)
			for(byte i=0;i<5;i++)
				m_worm->tProfile->sWeaponSlots[i] = m_worm->tWeapons[i].Weapon->Name;
		}
	}
	
	
	
    y+=5;
	if(m_worm->iCurrentWeapon == m_worm->iNumWeaponSlots)
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionActive, "Random");
	else
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionDefault, "Random");
	
    y+=18;
	
	if(m_worm->iCurrentWeapon == m_worm->iNumWeaponSlots+1)
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionActive, "Done");
	else
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionDefault, "Done");
	
	
	// list current key settings
	// TODO: move this out here
	y += 20;
	tLX->cFont.DrawCentre(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "~ Key settings ~");
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "up/down: " + cUp.getEventName() + "/" + cDown.getEventName());
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "left/right: " + cLeft.getEventName() + "/" + cRight.getEventName());
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "shoot: " + cShoot.getEventName());
	y -= 45;
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "jump/ninja: " + cJump.getEventName() + "/" + cInpRope.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "select weapon: " + cSelWeapon.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "strafe: " + cStrafe.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "quick select weapon: " + cWeapons[0].getEventName() + " " + cWeapons[1].getEventName() + " " + cWeapons[2].getEventName() + " " + cWeapons[3].getEventName() + " " + cWeapons[4].getEventName() );
	
	
	if(!bChat_Typing) {
		// move selection up or down
		if (cDown.isJoystickThrottle() || cUp.isJoystickThrottle())  {
			m_worm->iCurrentWeapon = (cUp.getJoystickValue() + 32768) * (m_worm->iNumWeaponSlots + 2) / 65536; // We have 7 rows and 65536 throttle states
			
		} else {
			int change = cDown.wasDown() - cUp.wasDown();
			m_worm->iCurrentWeapon += change;
			m_worm->iCurrentWeapon %= m_worm->iNumWeaponSlots + 2;
			if(m_worm->iCurrentWeapon < 0) m_worm->iCurrentWeapon += m_worm->iNumWeaponSlots + 2;
		}
	}
}
