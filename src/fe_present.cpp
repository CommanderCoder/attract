/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_image.hpp"
#include "fe_text.hpp"
#include "fe_sound.hpp"
#include "fe_input.hpp"

#include <sqrat.h>

#ifdef ENABLE_SCRIPT_SYSTEM_ACCESS
#include <sqstdblob.h>
#include <sqstdio.h>
#include <sqstdsystem.h>
#endif 

#include <sqstdmath.h>
#include <sqstdstring.h>

#include <iostream>
#include <stdio.h>
#include <ctime>
#include <stdarg.h>

FePresent::FePresent( FeSettings *fesettings, sf::Font &defaultfont )
	: m_feSettings( fesettings ), 
	m_currentFont( NULL ), 
	m_defaultFont( defaultfont ), 
	m_moveState( MoveNone ), 
	m_baseRotation( FeSettings::RotateNone ), 
	m_toggleRotation( FeSettings::RotateNone ), 
	m_playMovies( true ), 
	m_screenSaverActive( false ), 
	m_listBox( NULL )
{
	Sqrat::DefaultVM::Set( NULL );

	srand( time( NULL ) );
}

FePresent::~FePresent() 
{
	clear();
	vm_close();
}

void FePresent::clear()
{
	//
	// keep toggle rotation state and mute state through clear
	//
	m_listBox=NULL; // listbox gets deleted with the m_elements below
	m_moveState = MoveNone;
	m_baseRotation = FeSettings::RotateNone;
	m_scaleTransform = m_rotationTransform = sf::Transform();
	m_currentFont = &m_defaultFont;
	m_layoutFontName.clear();
	m_ticksList.clear();
	m_transitionList.clear();

	while ( !m_elements.empty() )
	{
		FeBasePresentable *p = m_elements.back();
		m_elements.pop_back();
		delete p;
	}

	while ( !m_texturePool.empty() )
	{
		FeTextureContainer *t = m_texturePool.back();
		m_texturePool.pop_back();
		delete t;
	}

	while ( !m_scriptSounds.empty() )
	{
		FeScriptSound *s = m_scriptSounds.back();
		m_scriptSounds.pop_back();
		delete s;
	}

	sf::VideoMode vm = sf::VideoMode::getDesktopMode();
	m_layoutSize.x = vm.width;
	m_layoutSize.y = vm.height;
}

void FePresent::draw( sf::RenderTarget& target, sf::RenderStates states ) const
{
	states.transform = m_rotationTransform * m_scaleTransform;

	std::vector<FeBasePresentable *>::const_iterator itl;
	for ( itl=m_elements.begin(); itl != m_elements.end(); ++itl )
	{
		if ( (*itl)->get_visible() )
			target.draw( (*itl)->drawable(), states );
	}
}

FeImage *FePresent::add_image( bool is_artwork, const std::string &n, int x, int y, int w, int h )
{
	std::string name;
	if ( is_artwork )
		name = n;
	else
		name = m_feSettings->get_current_layout_dir() + n;

	FeTextureContainer *new_tex = new FeTextureContainer( is_artwork, name );
	FeImage *new_image = new FeImage( new_tex );
	new_image->setPosition( x, y );
	new_image->setSize( w, h );

	m_redrawTriggered = true;
	m_texturePool.push_back( new_tex );
	m_elements.push_back( new_image );

	return new_image;
}

FeImage *FePresent::add_clone( FeImage *o )
{
	FeImage *new_image = new FeImage( o );
	m_redrawTriggered = true;
	m_elements.push_back( new_image );
	return new_image;
}

FeText *FePresent::add_text( const std::string &n, int x, int y, int w, int h )
{
	FeText *new_text = new FeText( n );
	new_text->setPosition( x, y );
	new_text->setSize( w, h );

	if ( m_currentFont )
		new_text->setFont( *m_currentFont );

	m_redrawTriggered = true;
	m_elements.push_back( new_text );
	return new_text;
}

FeListBox *FePresent::add_listbox( int x, int y, int w, int h )
{
	FeListBox *new_lb = new FeListBox();
	new_lb->setPosition( x, y );
	new_lb->setSize( w, h );

	if ( m_currentFont )
		new_lb->setFont( *m_currentFont );

	m_redrawTriggered = true;
	m_listBox = new_lb;
	m_elements.push_back( new_lb );
	return new_lb;
}

FeScriptSound *FePresent::add_sound( const std::string &n )
{
	std::string filename = m_feSettings->get_current_layout_dir();
	filename += n;

	FeScriptSound *new_sound = new FeScriptSound();
	new_sound->load( filename );
	new_sound->set_volume( 
		m_feSettings->get_play_volume( FeSoundInfo::Sound ) );

	m_scriptSounds.push_back( new_sound );
	return new_sound;
}

void FePresent::add_ticks_callback( const std::string &n )
{
	m_ticksList.push_back( n );	
}

void FePresent::add_transition_callback( const std::string &n )
{
	m_transitionList.push_back( n );	
}

int FePresent::get_layout_width() const
{
	return m_layoutSize.x;
}

int FePresent::get_layout_height() const
{
	return m_layoutSize.y;
}

void FePresent::set_layout_width( int w )
{
	m_layoutSize.x = w;
	m_scaleTransform.scale( 
		(float) sf::VideoMode::getDesktopMode().width / w, 1.0 );

	m_redrawTriggered = true;
}

void FePresent::set_layout_height( int h )
{
	m_layoutSize.y = h;
	m_scaleTransform.scale( 1.0,
		(float) sf::VideoMode::getDesktopMode().height / h );

	m_redrawTriggered = true;
}

void FePresent::set_layout_font( const char *n )
{
	m_layoutFontName = n;
	std::string filename;
	if ( m_feSettings->get_font_file( filename, m_layoutFontName ) )
	{
		if ( m_layoutFont.loadFromFile( filename ) )
		{
			m_currentFont = &m_layoutFont;
			m_redrawTriggered = true;
		}
	}
}

const char *FePresent::get_layout_font() const
{
	return m_layoutFontName.c_str();
}

void FePresent::set_layout_orient( int r )
{
	m_baseRotation = (FeSettings::RotationState)r;
	set_rotation_transform();
	m_redrawTriggered = true;
}

int FePresent::get_layout_orient() const
{
	return m_baseRotation;
}

bool FePresent::handle_event( FeInputMap::Command c, 
	sf::Event ev,
	sf::RenderWindow *wnd )
{
	m_moveState=MoveNone;
	m_lastInput=m_layoutTimer.getElapsedTime();

	if ( m_screenSaverActive )
	{
		// Reset from screen saver
		//
		load_layout( wnd );
		return true;
	}

	switch( c )
	{
	case FeInputMap::Down:
		m_moveTimer.restart();
		m_moveState=MoveDown;
		m_moveEvent = ev;
		vm_on_transition( ToNewSelection, 1, wnd );
		m_feSettings->change_rom( 1 );
		update( false );
		break;

	case FeInputMap::Up:
		m_moveTimer.restart();
		m_moveState=MoveUp;
		m_moveEvent = ev;
		vm_on_transition( ToNewSelection, -1, wnd );
		m_feSettings->change_rom( -1 );
		update( false );
		break;

	case FeInputMap::PageDown:
		m_moveTimer.restart();
		m_moveState=MovePageDown;
		m_moveEvent = ev;
		{
			int page = get_page_size();
			vm_on_transition( ToNewSelection, page, wnd );
			m_feSettings->change_rom( page, false );
		}
		update( false );
		break;

	case FeInputMap::PageUp:
		m_moveTimer.restart();
		m_moveState=MovePageUp;
		m_moveEvent = ev;
		{
			int page = -1 * get_page_size();
			vm_on_transition( ToNewSelection, page, wnd );
			m_feSettings->change_rom( page, false );
		}
		update( false );
		break;

	case FeInputMap::ToggleRotateRight:
		toggle_rotate( FeSettings::RotateRight );
		break;

	case FeInputMap::ToggleFlip:
		toggle_rotate( FeSettings::RotateFlip );
		break;

	case FeInputMap::ToggleRotateLeft:
		toggle_rotate( FeSettings::RotateLeft );
		break;

	case FeInputMap::ToggleMovie:
		toggle_movie();
		break;

	case FeInputMap::NextList:
		// next_list returns true if the layout changes with the new list
		//
		if ( m_feSettings->next_list() ) 
			load_layout( wnd );
		else
			update( true );

		break;

	case FeInputMap::PrevList:
		// prev_list returns true if the layout changes with the new list
		//
		if ( m_feSettings->prev_list() )
			load_layout( wnd );
		else
			update( true );

		break;

	case FeInputMap::ToggleLayout:
		m_feSettings->toggle_layout();
		load_layout( wnd );
		break;

	case FeInputMap::LAST_COMMAND:
	default:
		// Not handled by us, return false so calling function knows
		//
		return false;
	}

	return true;
}

int FePresent::update( bool new_list ) 
{
	std::vector<FeBasePresentable *>::iterator itl;
	if ( new_list )
	{
		for ( itl=m_elements.begin(); itl != m_elements.end(); ++itl )
			(*itl)->on_new_list( m_feSettings );
	}

	std::vector<FeTextureContainer *>::iterator itc;
	for ( itc=m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
		(*itc)->on_new_selection( m_feSettings );

	for ( itl=m_elements.begin(); itl != m_elements.end(); ++itl )
		(*itl)->on_new_selection( m_feSettings );

	m_movieStartTimer.restart();

	return 0;
}

void FePresent::load_screensaver( sf::RenderWindow *wnd ) 
{
	bool from_screenSaver = m_screenSaverActive;
	vm_on_transition( EndLayout, 1, wnd );
	clear();
	set_rotation_transform();
	m_screenSaverActive=true;

	//
	// Run the script which actually sets up the screensaver
	//
	m_layoutTimer.restart();
	vm_on_new_layout( m_feSettings->get_screensaver_file() );

	//
	// if there is no screen saver script then do a blank screen
	//
	update( true );
	vm_on_transition( StartLayout, (from_screenSaver?1:0), wnd );
}

void FePresent::load_layout( sf::RenderWindow *wnd ) 
{
	bool from_screenSaver = m_screenSaverActive;
	vm_on_transition( EndLayout, 0, wnd );
	clear();
	set_rotation_transform();
	m_screenSaverActive=false;

	sf::VideoMode vm = sf::VideoMode::getDesktopMode();
	if ( m_feSettings->lists_count() < 1 )
	{
		std::string msg;
		m_feSettings->get_resource( "No lists configured.", msg );
		add_text( msg, 0, 0, vm.width, vm.height );
		update( true );
		return;
	}

	//
	// Run the script which actually sets up the layout
	//
	m_layoutTimer.restart();
	vm_on_new_layout( m_feSettings->get_current_layout_file() );

	// make things usable if the layout is empty
	if ( m_elements.empty() )
	{
		//
		// Nothing loaded, default to a full screen list with the configured
		// movie artwork as the background
		//
		FeImage *img = cb_add_artwork( "", 0, 0, vm.width, vm.height );
		img->setColor( sf::Color( 100, 100, 100, 180 ) );
		cb_add_listbox( 0, 0, vm.width, vm.height );
	}

	update( true );
	vm_on_transition( StartLayout, (from_screenSaver?1:0), wnd );
}

bool FePresent::tick( sf::RenderWindow *wnd )
{
	bool ret_val = false;
	if ( m_moveState != MoveNone )
	{
		sf::Time t = m_moveTimer.getElapsedTime();
		if ( t.asMilliseconds() > 500 )
		{
			bool cont=false;

			if ( m_moveEvent.type == sf::Event::KeyPressed )
			{
				if ( sf::Keyboard::isKeyPressed( m_moveEvent.key.code ) )
					cont=true;
			}
			else if ( m_moveEvent.type == sf::Event::JoystickButtonPressed )
			{
				if ( sf::Joystick::isButtonPressed( 
						m_moveEvent.joystickButton.joystickId,
						m_moveEvent.joystickButton.button ) )
					cont=true;
			}
			else if ( m_moveEvent.type == sf::Event::JoystickMoved )
			{
				float pos = sf::Joystick::getAxisPosition( 
						m_moveEvent.joystickMove.joystickId,
						m_moveEvent.joystickMove.axis );
				if ( abs( pos ) > FeInputMap::JOY_THRESH )
					cont=true;
			}

			if ( cont )
			{
				int offset( 0 );
				switch ( m_moveState )
				{
					case MoveUp: offset = -1; break;
					case MoveDown: offset = 1; break;
					case MovePageUp: offset = -1 * get_page_size(); break;
					case MovePageDown: offset = get_page_size(); break;
					default: break;
				}

				vm_on_transition( ToNewSelection, offset, wnd );
				m_feSettings->change_rom( offset, false ); 

				ret_val=true;
				update( false );
			}
			else
			{
				m_moveState = MoveNone;
			}
		}
	}

	//
	// Start movies after a small delay
	//
	int time = m_movieStartTimer.getElapsedTime().asMilliseconds();
	if (( time > 500 ) && ( m_playMovies ))
	{
		for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		{
			if ( (*itm)->tick( m_feSettings ) )
				ret_val=true;
		}
	}

	if ( vm_on_tick())
		ret_val = true;

	//
	// Only check to switch to screensaver if wnd is not NULL
	// we are given a NULL wnd value when the overlay (config menu)
	// is being displayed
	//
	if ( wnd )
	{
		int saver_timeout = m_feSettings->get_screen_saver_timeout();
		if (( !m_screenSaverActive ) && ( saver_timeout > 0 ))
		{
		 	if ( ( m_layoutTimer.getElapsedTime() - m_lastInput ) 
					> sf::seconds( saver_timeout ) )
			{
				load_screensaver( wnd );
				ret_val = true;
			}
		}
	}

	return ret_val;
}

int FePresent::get_page_size() const
{
	if ( m_listBox )
		return m_listBox->getRowCount();
	else
		return 5;
}

void FePresent::stop( sf::RenderWindow *wnd )
{
	for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_play_state( false );

	vm_on_transition( EndLayout, 0, wnd );
}

void FePresent::pre_run( sf::RenderWindow *wnd )
{
	for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_play_state( false );

	vm_on_transition( ToGame, 0, wnd );
}

void FePresent::post_run( sf::RenderWindow *wnd )
{
	perform_autorotate();
	vm_on_transition( FromGame, 0, wnd );

	for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_play_state( m_playMovies );
}

void FePresent::toggle_movie()
{
	m_playMovies = !m_playMovies;

	for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_play_state( m_playMovies );
}

void FePresent::toggle_mute()
{
	for ( std::vector<FeTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_vol( m_feSettings->get_play_volume( FeSoundInfo::Movie ) );

	for ( std::vector<FeScriptSound *>::iterator its=m_scriptSounds.begin();
				its != m_scriptSounds.end(); ++its )
		(*its)->set_volume( m_feSettings->get_play_volume( FeSoundInfo::Sound ) );
}

const sf::Transform &FePresent::get_rotation_transform() const
{
	return m_rotationTransform;
}

const sf::Font *FePresent::get_font() const
{
	return m_currentFont;
}

void FePresent::set_default_font( sf::Font &f )
{
	m_defaultFont = f;
}

void FePresent::toggle_rotate( FeSettings::RotationState r )
{
	if ( m_toggleRotation != FeSettings::RotateNone )
		m_toggleRotation = FeSettings::RotateNone;
	else
		m_toggleRotation = r;

	set_rotation_transform();
}

void FePresent::set_rotation_transform()
{
	sf::VideoMode vm = sf::VideoMode::getDesktopMode();
	m_rotationTransform = sf::Transform();

	FeSettings::RotationState actualRotation 
		= (FeSettings::RotationState)(( m_baseRotation + m_toggleRotation ) % 4);

	switch ( actualRotation )
	{
	case FeSettings::RotateNone:
		// do nothing
		break;
	case FeSettings::RotateRight:
		m_rotationTransform.translate( vm.width, 0 );
		m_rotationTransform.scale( (float) vm.width / vm.height,
												(float) vm.height / vm.width );
		m_rotationTransform.rotate(90);
		break;
	case FeSettings::RotateFlip:
		m_rotationTransform.translate( vm.width, vm.height );
		m_rotationTransform.rotate(180);
		break;
	case FeSettings::RotateLeft:
		m_rotationTransform.translate( 0, vm.height );
		m_rotationTransform.scale( (float) vm.width / vm.height,
											(float) vm.height / vm.width );
		m_rotationTransform.rotate(270);
		break;
	}
}

void FePresent::perform_autorotate()
{
	FeSettings::RotationState autorotate = m_feSettings->get_autorotate();

	if ( autorotate == FeSettings::RotateNone )
		return;

	std::string rom_rot = m_feSettings->get_rom_info( 0, 
						FeRomInfo::Rotation );

	m_toggleRotation = FeSettings::RotateNone;

	switch ( m_baseRotation )
	{
	case FeSettings::RotateLeft:
	case FeSettings::RotateRight:
		if (( rom_rot.compare( "0" ) == 0 ) || ( rom_rot.compare( "180" ) == 0 ))
			m_toggleRotation = autorotate;
		break;
	case FeSettings::RotateNone:
	case FeSettings::RotateFlip:
	default:
		if (( rom_rot.compare( "90" ) == 0 ) || ( rom_rot.compare( "270" ) == 0 ))
			m_toggleRotation = autorotate;
	}

	set_rotation_transform();
}

//
// Squirrel callback functions
//
void printFunc(HSQUIRRELVM v, const SQChar *s, ...)
{
	std::cout << "Script: ";

	va_list vl;
	va_start(vl, s);
	vprintf(s, vl);
	va_end(vl);

	std::cout << std::endl;
}

FeImage* FePresent::cb_add_image(const char *n, int x, int y, int w, int h )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	FeImage *ret = fep->add_image( false, n, x, y, w, h );

	// Add the image to the "fe.obj" table in Squirrel
	//
	Sqrat::Object fe( Sqrat::RootTable().GetSlot( _SC("fe") ) );
	Sqrat::Table obj( fe.GetSlot( _SC("obj") ) );
	obj.SetInstance( obj.GetSize(), ret );

	return ret;
}

FeImage* FePresent::cb_add_image(const char *n, int x, int y )
{
	return cb_add_image( n, x, y, 0, 0 );
}

FeImage* FePresent::cb_add_image(const char *n )
{
	return cb_add_image( n, 0, 0, 0, 0 );
}

FeImage* FePresent::cb_add_artwork(const char *n, int x, int y, int w, int h )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	FeImage *ret = fep->add_image( true, n, x, y, w, h );

	// Add the image to the "fe.obj" table in Squirrel
	//
	Sqrat::Object fe( Sqrat::RootTable().GetSlot( _SC("fe") ) );
	Sqrat::Table obj( fe.GetSlot( _SC("obj") ) );
	obj.SetInstance( obj.GetSize(), ret );

	return ret;
}

FeImage* FePresent::cb_add_artwork(const char *n, int x, int y )
{
	return cb_add_artwork( n, x, y, 0, 0 );
}

FeImage* FePresent::cb_add_artwork(const char *n )
{
	return cb_add_artwork( n, 0, 0, 0, 0 );
}

FeImage* FePresent::cb_add_clone( FeImage *o )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	FeImage *ret = fep->add_clone( o );

	// Add the image to the "fe.obj" table in Squirrel
	//
	Sqrat::Object fe( Sqrat::RootTable().GetSlot( _SC("fe") ) );
	Sqrat::Table obj( fe.GetSlot( _SC("obj") ) );
	obj.SetInstance( obj.GetSize(), ret );

	return ret;
}

FeText* FePresent::cb_add_text(const char *n, int x, int y, int w, int h )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	FeText *ret = fep->add_text( n, x, y, w, h );

	// Add the text to the "fe.obj" table in Squirrel
	//
	Sqrat::Object fe( Sqrat::RootTable().GetSlot( _SC("fe") ) );
	Sqrat::Table obj( fe.GetSlot( _SC("obj") ) );
	obj.SetInstance( obj.GetSize(), ret );

	return ret;
}

FeText* FePresent::cb_add_text(const char *n, int x, int y )
{
	return cb_add_text( n, x, y, 0, 0 );
}

FeText* FePresent::cb_add_text(const char *n )
{
	return cb_add_text( n, 0, 0, 0, 0 );
}

FeListBox* FePresent::cb_add_listbox(int x, int y, int w, int h )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	FeListBox *ret = fep->add_listbox( x, y, w, h );

	// Add the listbox to the "fe.obj" table in Squirrel
	//
	Sqrat::Object fe ( Sqrat::RootTable().GetSlot( _SC("fe") ) );
	Sqrat::Table obj( fe.GetSlot( _SC("obj") ) );
	obj.SetInstance( obj.GetSize(), ret );

	return ret;
}

FeScriptSound* FePresent::cb_add_sound( const char *s )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	return fep->add_sound( s );
	//
	// We assume the script will keep a reference to the sound
	//
}

void FePresent::cb_add_ticks_callback( const char *n )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	fep->add_ticks_callback( n );
}

void FePresent::cb_add_transition_callback( const char *n )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );

	fep->add_transition_callback( n );
}

bool FePresent::cb_is_keypressed( int k )
{
	return sf::Keyboard::isKeyPressed( (sf::Keyboard::Key)k );
}

bool FePresent::cb_is_joybuttonpressed( int num, int b )
{
	return sf::Joystick::isButtonPressed( num, b );
}

float FePresent::cb_get_joyaxispos( int num, int a )
{
	return sf::Joystick::getAxisPosition( num, (sf::Joystick::Axis)a );
}

void FePresent::do_nut( const char *script_file )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );
	FeSettings *fes = fep->get_fes();

	std::string path = fes->get_current_layout_dir();
	path += script_file;

	if ( !file_exists( path ) )
	{
		std::cout << "File not found: " << path << std::endl;
	}
	else
	{
		try
		{
			Sqrat::Script sc;
			sc.CompileFile( path );
			sc.Run();
		}
		catch( Sqrat::Exception e )
		{
			std::cout << "Script Error in " << path
				<< " - " << e.Message() << std::endl;
		}
	}
}

const char *FePresent::cb_game_info( int index, int offset )
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	FePresent *fep = (FePresent *)sq_getforeignptr( vm );
	FeSettings *fes = fep->get_fes();

	return (fes->get_rom_info( offset, (FeRomInfo::Index)index )).c_str();
}

const char *FePresent::cb_game_info( int index )
{
	return cb_game_info( index, 0 );
}

void FePresent::flag_redraw()
{
	m_redrawTriggered=true;
}

void FePresent::vm_close()
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	if ( vm )
	{
		sq_close( vm );
		Sqrat::DefaultVM::Set( NULL );
	}
}

void FePresent::vm_init()
{
	vm_close();
	HSQUIRRELVM vm = sq_open( 1024 );
	sq_setprintfunc( vm, printFunc, printFunc );
	sq_pushroottable( vm );
	sq_setforeignptr( vm, this );

#ifdef ENABLE_SCRIPT_SYSTEM_ACCESS
	sqstd_register_bloblib( vm );
	sqstd_register_iolib( vm );
	sqstd_register_systemlib( vm );
#endif

	sqstd_register_mathlib( vm );
	sqstd_register_stringlib( vm );

#ifdef FE_DEBUG
	sqstd_seterrorhandlers( vm );
#endif

	Sqrat::DefaultVM::Set( vm );
}


void FePresent::vm_on_new_layout( const std::string &file )
{
	using namespace Sqrat;

	vm_close();

	// Squirrel VM gets reinitialized on each layout
	//
	vm_init();

	sf::VideoMode vm = sf::VideoMode::getDesktopMode();

	// Set fe-related constants
	//
	ConstTable()
		.Const( _SC("FeVersion"), FE_VERSION)
		.Const( _SC("FeVersionNum"), FE_VERSION_NUM)
		.Const( _SC("ScreenWidth"), (int)vm.width )
		.Const( _SC("ScreenHeight"), (int)vm.height )
		.Const( _SC("ScreenSaverActive"), m_screenSaverActive )
		.Enum( _SC("Transition"), Enumeration()
			.Const( _SC("StartLayout"), StartLayout )
			.Const( _SC("EndLayout"), EndLayout )
			.Const( _SC("ToNewSelection"), ToNewSelection )
			.Const( _SC("ToGame"), ToGame )
			.Const( _SC("FromGame"), FromGame )
			)
		.Enum( _SC("Style"), Enumeration()
			.Const( _SC("Regular"), sf::Text::Regular )
			.Const( _SC("Bold"), sf::Text::Bold )
			.Const( _SC("Italic"), sf::Text::Italic )
			.Const( _SC("Underlined"), sf::Text::Underlined )
			)
		.Enum( _SC("Align"), Enumeration()
			.Const( _SC("Left"), FeTextPrimative::Left )
			.Const( _SC("Centre"), FeTextPrimative::Centre )
			.Const( _SC("Right"), FeTextPrimative::Right )
			)
		.Enum( _SC("RotateScreen"), Enumeration()
			.Const( _SC("None"), FeSettings::RotateNone )
			.Const( _SC("Right"), FeSettings::RotateRight )
			.Const( _SC("Flip"), FeSettings::RotateFlip )
			.Const( _SC("Left"), FeSettings::RotateLeft )
			)
		.Enum( _SC("Axis"), Enumeration()
			.Const( _SC("X"), sf::Joystick::X )
			.Const( _SC("Y"), sf::Joystick::Y )
			.Const( _SC("Z"), sf::Joystick::Z )
			.Const( _SC("R"), sf::Joystick::R )
			.Const( _SC("U"), sf::Joystick::U )
			.Const( _SC("V"), sf::Joystick::V )
			.Const( _SC("PovX"), sf::Joystick::PovX )
			.Const( _SC("PovY"), sf::Joystick::PovY )
			)
		;

	Enumeration keys;
	int i=0;
	while ( FeInputMap::keyStrings[i] != NULL )
	{
		keys.Const( FeInputMap::keyStrings[i], i );
		i++;
	}

	ConstTable().Enum( _SC("Key"), keys);

	Enumeration info;
	i=0;
	while ( FeRomInfo::indexStrings[i] != NULL )
	{
		info.Const( FeRomInfo::indexStrings[i], i );
		i++;
	}
	ConstTable().Enum( _SC("Info"), info);

	//
	// Define classes for fe objects that get exposed to squirrel
	//

	// Base Presentable Object Class
	//
	RootTable().Bind( _SC("Presentable"), 
		Class<FeBasePresentable, NoConstructor>()
		.Prop(_SC("visible"), 
			&FeBasePresentable::get_visible, &FeBasePresentable::set_visible )
		.Prop(_SC("x"), &FeBasePresentable::get_x, &FeBasePresentable::set_x )
		.Prop(_SC("y"), &FeBasePresentable::get_y, &FeBasePresentable::set_y )
		.Prop(_SC("width"), 
			&FeBasePresentable::get_width, &FeBasePresentable::set_width )
		.Prop(_SC("height"), 
			&FeBasePresentable::get_height, &FeBasePresentable::set_height )
		.Prop(_SC("rotation"), 
			&FeBasePresentable::getRotation, &FeBasePresentable::setRotation )
		.Prop(_SC("red"), &FeBasePresentable::get_r, &FeBasePresentable::set_r )
		.Prop(_SC("green"), &FeBasePresentable::get_g, &FeBasePresentable::set_g )
		.Prop(_SC("blue"), &FeBasePresentable::get_b, &FeBasePresentable::set_b )
		.Prop(_SC("alpha"), &FeBasePresentable::get_a, &FeBasePresentable::set_a )
		.Prop(_SC("index_offset"), &FeBasePresentable::getIndexOffset, &FeBasePresentable::setIndexOffset )
		.Func( _SC("set_rgb"), &FeBasePresentable::set_rgb )
	);

	RootTable().Bind( _SC("Image"), 
		DerivedClass<FeImage, FeBasePresentable, NoConstructor>()
		.Prop(_SC("shear_x"), &FeImage::get_shear_x, &FeImage::set_shear_x )
		.Prop(_SC("shear_y"), &FeImage::get_shear_y, &FeImage::set_shear_y )
		.Prop(_SC("texture_width"), &FeImage::get_texture_width )
		.Prop(_SC("texture_height"), &FeImage::get_texture_height )
		.Prop(_SC("subimg_x"), &FeImage::get_subimg_x, &FeImage::set_subimg_x )
		.Prop(_SC("subimg_y"), &FeImage::get_subimg_y, &FeImage::set_subimg_y )
		.Prop(_SC("subimg_width"), &FeImage::get_subimg_width, &FeImage::set_subimg_width )
		.Prop(_SC("subimg_height"), &FeImage::get_subimg_height, &FeImage::set_subimg_height )
		.Prop(_SC("movie_enabled"), &FeImage::getMovieEnabled, &FeImage::setMovieEnabled )
	);

	RootTable().Bind( _SC("Text"), 
		DerivedClass<FeText, FeBasePresentable, NoConstructor>()
		.Prop(_SC("msg"), &FeText::get_string, &FeText::set_string )
		.Prop(_SC("bg_red"), &FeText::get_bgr, &FeText::set_bgr )
		.Prop(_SC("bg_green"), &FeText::get_bgg, &FeText::set_bgg )
		.Prop(_SC("bg_blue"), &FeText::get_bgb, &FeText::set_bgb )
		.Prop(_SC("bg_alpha"), &FeText::get_bga, &FeText::set_bga )
		.Prop(_SC("charsize"), &FeText::get_charsize, &FeText::set_charsize )
		.Prop(_SC("style"), &FeText::get_style, &FeText::set_style )
		.Prop(_SC("align"), &FeText::get_align, &FeText::set_align )
		.Func( _SC("set_bg_rgb"), &FeText::set_bg_rgb )
	);

	RootTable().Bind( _SC("ListBox"), 
		DerivedClass<FeListBox, FeBasePresentable, NoConstructor>()
		.Prop(_SC("bg_red"), &FeListBox::get_bgr, &FeListBox::set_bgr )
		.Prop(_SC("bg_green"), &FeListBox::get_bgg, &FeListBox::set_bgg )
		.Prop(_SC("bg_blue"), &FeListBox::get_bgb, &FeListBox::set_bgb )
		.Prop(_SC("bg_alpha"), &FeListBox::get_bga, &FeListBox::set_bga )
		.Prop(_SC("sel_red"), &FeListBox::get_selr, &FeListBox::set_selr )
		.Prop(_SC("sel_green"), &FeListBox::get_selg, &FeListBox::set_selg )
		.Prop(_SC("sel_blue"), &FeListBox::get_selb, &FeListBox::set_selb )
		.Prop(_SC("sel_alpha"), &FeListBox::get_sela, &FeListBox::set_sela )
		.Prop(_SC("selbg_red"), &FeListBox::get_selbgr, &FeListBox::set_selbgr )
		.Prop(_SC("selbg_green"), &FeListBox::get_selbgg, &FeListBox::set_selbgg )
		.Prop(_SC("selbg_blue"), &FeListBox::get_selbgb, &FeListBox::set_selbgb )
		.Prop(_SC("selbg_alpha"), &FeListBox::get_selbga, &FeListBox::set_selbga )
		.Prop(_SC("charsize"), &FeListBox::get_charsize, &FeListBox::set_charsize )
		.Prop(_SC("style"), &FeListBox::get_style, &FeListBox::set_style )
		.Prop(_SC("align"), &FeListBox::get_align, &FeListBox::set_align )
		.Prop(_SC("sel_style"), &FeListBox::getSelStyle, &FeListBox::setSelStyle )
		.Func( _SC("set_bg_rgb"), &FeListBox::set_bg_rgb )
		.Func( _SC("set_sel_rgb"), &FeListBox::set_sel_rgb )
		.Func( _SC("set_selbg_rgb"), &FeListBox::set_selbg_rgb )
	);

	RootTable().Bind( _SC("LayoutGlobals"), Class <FePresent, NoConstructor>()
		.Prop( _SC("width"), &FePresent::get_layout_width, &FePresent::set_layout_width )
		.Prop( _SC("height"), &FePresent::get_layout_height, &FePresent::set_layout_height )
		.Prop( _SC("font"), &FePresent::get_layout_font, &FePresent::set_layout_font )
		.Prop( _SC("orient"), &FePresent::get_layout_orient, &FePresent::set_layout_orient )
	);

	RootTable().Bind( _SC("Sound"), Class <FeScriptSound, NoConstructor>()
		.Func( _SC("load"), &FeScriptSound::load )
		.Func( _SC("play"), &FeScriptSound::play )
		.Prop( _SC("is_playing"), &FeScriptSound::is_playing )
		.Prop( _SC("pitch"), &FeScriptSound::get_pitch, &FeScriptSound::set_pitch )
		.Prop( _SC("x"), &FeScriptSound::get_x, &FeScriptSound::set_x )
		.Prop( _SC("y"), &FeScriptSound::get_y, &FeScriptSound::set_y )
		.Prop( _SC("z"), &FeScriptSound::get_z, &FeScriptSound::set_z )
	);

	// All frontend functionality is in the "fe" table in Squirrel
	//
	Table fe;
	fe.Overload<FeImage* (*)(const char *, int, int, int, int)>(_SC("add_image"), &FePresent::cb_add_image);
	fe.Overload<FeImage* (*)(const char *, int, int)>(_SC("add_image"), &FePresent::cb_add_image);
	fe.Overload<FeImage* (*)(const char *)>(_SC("add_image"), &FePresent::cb_add_image);

	fe.Overload<FeImage* (*)(const char *, int, int, int, int)>(_SC("add_artwork"), &FePresent::cb_add_artwork);
	fe.Overload<FeImage* (*)(const char *, int, int)>(_SC("add_artwork"), &FePresent::cb_add_artwork);
	fe.Overload<FeImage* (*)(const char *)>(_SC("add_artwork"), &FePresent::cb_add_artwork);

	fe.Overload<FeImage* (*)(const char *)>(_SC("add_artwork"), &FePresent::cb_add_artwork);
	
	fe.Func<FeImage* (*)(FeImage *)>(_SC("add_clone"), &FePresent::cb_add_clone);

	fe.Overload<FeText* (*)(const char *, int, int, int, int)>(_SC("add_text"), &FePresent::cb_add_text);
	fe.Overload<FeText* (*)(const char *, int, int)>(_SC("add_text"), &FePresent::cb_add_text);
	fe.Overload<FeText* (*)(const char *)>(_SC("add_text"), &FePresent::cb_add_text);

	fe.Func<FeListBox* (*)(int, int, int, int)>(_SC("add_listbox"), &FePresent::cb_add_listbox);
	fe.Func<FeScriptSound* (*)(const char *)>(_SC("add_sound"), &FePresent::cb_add_sound);
	fe.Func<void (*)(const char *)>(_SC("add_ticks_callback"), &FePresent::cb_add_ticks_callback);
	fe.Func<void (*)(const char *)>(_SC("add_transition_callback"), &FePresent::cb_add_transition_callback);
	fe.Func<bool (*)(int)>(_SC("is_keypressed"), &FePresent::cb_is_keypressed);
	fe.Func<bool (*)(int, int)>(_SC("is_joybuttonpressed"), &FePresent::cb_is_joybuttonpressed);
	fe.Func<float (*)(int, int)>(_SC("get_joyaxispos"), &FePresent::cb_get_joyaxispos);
	fe.Func<void (*)(const char *)>(_SC("do_nut"), &FePresent::do_nut);
	fe.Overload<const char* (*)(int)>(_SC("game_info"), &FePresent::cb_game_info);
	fe.Overload<const char* (*)(int, int)>(_SC("game_info"), &FePresent::cb_game_info);

	fe.SetInstance( _SC("layout"), this );

	// Each presentation object gets an instance in the
	// "obj" table available in Squirrel
	//
	Table obj;
	fe.Bind( _SC("obj"), obj );
	RootTable().Bind( _SC("fe"),  fe );

	//
	// Now run the global script, followed by the layout script
	//
	std::string global = m_feSettings->get_layout_global_file();
	if ( file_exists( global ) )
	{
		try
		{
			Script sc;
			sc.CompileFile( global );
			sc.Run();
		}
		catch( Exception e )
		{
			std::cout << "Script Error in " << global
				<< " - " << e.Message() << std::endl;
		}
	}

	if ( file_exists( file ) )
	{
		try
		{
			Script sc;
			sc.CompileFile( file );
			sc.Run();
		}
		catch( Exception e )
		{
			std::cout << "Script Error in " << file 
				<< " - " << e.Message() << std::endl;
		}
	}
}

bool FePresent::vm_on_tick()
{
	using namespace Sqrat;
	m_redrawTriggered = false;

	for ( std::vector<std::string>::iterator itr = m_ticksList.begin();
		itr != m_ticksList.end(); ++itr )
	{
		// Assumption: Ticks list is empty if no vm is active
		//
		ASSERT( Sqrat::DefaultVM::Get() );

		try 
		{
			Function func( RootTable(), (*itr).c_str());

			if ( !func.IsNull() )
				func.Execute( m_layoutTimer.getElapsedTime().asMilliseconds() );
		}
		catch( Exception e )
		{
			std::cout << "Script Error: " << e.Message() << std::endl;
		}
	}

	return m_redrawTriggered;
}

bool FePresent::vm_on_transition( 
	FeTransitionType t, 
	int var,
	sf::RenderWindow *wnd )
{
	using namespace Sqrat;

	sf::Time tstart = m_layoutTimer.getElapsedTime();
	m_redrawTriggered = false;

	for ( std::vector<std::string>::iterator itr = m_transitionList.begin();
		itr != m_transitionList.end(); ++itr )
	{
		// Assumption: Transition list is empty if no vm is active
		//
		ASSERT( Sqrat::DefaultVM::Get() );

		try 
		{
			Function func( RootTable(), (*itr).c_str());
			if ( !func.IsNull() )
			{
				sf::Time ttime = m_layoutTimer.getElapsedTime() - tstart;

				while (( func.Evaluate<bool>( (int)t,
					var,
					ttime.asMilliseconds() ) == true ) && ( wnd ))
				{
					// redraw and reevaluate function if it returns true
					//
					wnd->clear();
					wnd->draw( *this );
					wnd->display();
					m_redrawTriggered = false; // clear redraw flag

					ttime = m_layoutTimer.getElapsedTime() - tstart;
				}
			}
		}
		catch( Exception e )
		{
			std::cout << "Script Error: " << e.Message() << std::endl;
		}
	}

	return m_redrawTriggered;
}

namespace 
{
	FePresent *helper_get_fep()
	{
		HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

		if ( !vm )
			return NULL;

		return (FePresent *)sq_getforeignptr( vm );
	}
};

void script_do_update( FeBasePresentable *bp )
{
	FePresent *fep = helper_get_fep();

	if ( fep )
		bp->on_new_selection( fep->get_fes() );
}

void script_do_update( FeTextureContainer *tc )
{
	FePresent *fep = helper_get_fep();

	if ( fep )
		tc->on_new_selection( fep->get_fes() );
}

void script_flag_redraw()
{
	FePresent *fep = helper_get_fep();

	if ( fep )
		fep->flag_redraw();
}