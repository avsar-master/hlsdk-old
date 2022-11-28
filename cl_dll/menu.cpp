/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// menu.cpp
//
// generic menu handler
//
#include "hud.h"
#include "util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

int menu_r, menu_g, menu_b, menu_x;
int menu_ralign;

#define MAX_MENU_STRING	512
char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

DECLARE_MESSAGE( m_Menu, ShowMenu );

int CHudMenu::Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( ShowMenu );

	InitHUDData();

	return 1;
}

void CHudMenu::InitHUDData( void )
{
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;
	Reset();
}

void CHudMenu::Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = FALSE;
}

int CHudMenu::VidInit( void )
{
	return 1;
}

int CHudMenu::Draw( float flTime )
{
	// check for if menu is set to disappear
	if (m_flShutoffTime > 0)
	{
		if (m_flShutoffTime <= gHUD.m_flTime)
		{
			m_fMenuDisplayed = 0;
			m_iFlags &= ~HUD_ACTIVE;
			return 1;
		}
	}

	int nlc = 0;

	for (int i = 0; i < MAX_MENU_STRING && g_szMenuString[i] != '\0'; i++)
	{
		if (g_szMenuString[i] == '\n')
			nlc++;
	}
	
	menu_x = 20; menu_r = 255; menu_g = 255; menu_b = 255;
	menu_ralign = 0;

	int y = (ScreenHeight / 2) - ((nlc / 2) * 12) - 40;
	const char *sptr = g_szMenuString;
	int i;
	char menubuf[80];
	const char *ptr;

	while (*sptr){

		if (*sptr == '\\'){

			switch (*(sptr + 1)){
				case '\0':{
					sptr += 1;
					break;
				}
				case 'R':{
					menu_ralign = 1; menu_x = 299; // 299?? menu_x = ScreenWidth / 2
					sptr += 2;
					break;
				}
				case 'd':{
					menu_r = 100; menu_g = 100; menu_b = 100;
					sptr += 2;
					break;
				}
				case 'r':{
					menu_r = 210; menu_g = 24; menu_b = 0;
					sptr += 2;
					break;
				}
				case 'w':{
					menu_r = 255; menu_g = 255; menu_b = 255;
					sptr += 2;
					break;
				}
				case 'y':{
					menu_r = 255; menu_g = 210; menu_b = 64;
					sptr += 2;
					break;
				}
				default:{
					sptr += 2;
				}
			}
			continue;
		}

		if (*sptr == '\n')
		{
			menu_ralign = 0;
			menu_x = 20;
			y += 12;
			sptr += 1;
			continue;
		}

		for (ptr = sptr; *sptr != '\0'; sptr++)
		{
			if (*sptr == '\n')
				break;

			if (*sptr == '\\')
				break;
		}

		i = sptr - ptr;
		strncpy(menubuf, ptr, min(i, sizeof(menubuf)));
		menubuf[min(i, sizeof(menubuf) - 1)] = 0;

		if (menu_ralign)
			menu_x = gHUD.DrawHudStringReverse(menu_x, y, 0, menubuf, menu_r, menu_g, menu_b);
		else
			menu_x = gHUD.DrawHudString(menu_x, y, 320, menubuf, menu_r, menu_g, menu_b);
	}
	
	return 1;
}

// selects an item from the menu
void CHudMenu::SelectMenuItem( int menu_item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		sprintf( szbuf, "menuselect %d\n", menu_item );
		ClientCmd( szbuf );

		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
	}
}


// Message handler for ShowMenu message
// takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, FALSE if it's the last string
//		string: menu string to display
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudMenu::MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_bitsValidSlots = READ_SHORT();
	int DisplayTime = READ_CHAR();
	int NeedMore = READ_BYTE();

	if ( DisplayTime > 0 )
		m_flShutoffTime = DisplayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if ( m_bitsValidSlots )
	{
		if ( !m_fWaitingForMore ) // this is the start of a new menu
		{
			strncpy( g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING );
		}
		else
		{  // append to the current menu string
			strncat( g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING - strlen(g_szPrelocalisedMenuString) );
		}
		g_szPrelocalisedMenuString[MAX_MENU_STRING-1] = 0;  // ensure null termination (strncat/strncpy does not)

		if ( !NeedMore )
		{  // we have the whole string, so we can localise it now
			strcpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedMenuString ) );
		}

		m_fMenuDisplayed = 1;
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		m_fMenuDisplayed = 0; // no valid slots means that the menu should be turned off
		m_iFlags &= ~HUD_ACTIVE;
	}

	m_fWaitingForMore = NeedMore;

	return 1;
}
