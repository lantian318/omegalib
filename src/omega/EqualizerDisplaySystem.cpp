/******************************************************************************
 * THE OMEGA LIB PROJECT
 *-----------------------------------------------------------------------------
 * Copyright 2010-2013		Electronic Visualization Laboratory, 
 *							University of Illinois at Chicago
 * Authors:										
 *  Alessandro Febretti		febret@gmail.com
 *-----------------------------------------------------------------------------
 * Copyright (c) 2010-2013, Electronic Visualization Laboratory,  
 * University of Illinois at Chicago
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this 
 * list of conditions and the following disclaimer. Redistributions in binary 
 * form must reproduce the above copyright notice, this list of conditions and 
 * the following disclaimer in the documentation and/or other materials provided 
 * with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-----------------------------------------------------------------------------
 * What's in this file
 *	The wrapper code for the omegalib / Equalizer interface.
 * 	This file contains most of the setup code for the wrapper: equalizer
 *      config generation, startup, shutdown, plus some utility function to 
 * 	convert 2d points to 3d rays.
 ******************************************************************************/
#include <omegaGl.h>

#include "eqinternal/eqinternal.h"

#include "omega/EqualizerDisplaySystem.h"
#include "omega/SystemManager.h"
#include "omega/MouseService.h"

using namespace omega;
using namespace co::base;
using namespace std;

#define OMEGA_EQ_TMP_FILE "./_eqcfg.eqc"

#define L(line) indent + line + "\n"
#define START_BLOCK(string, name) string += indent + name + "\n" + indent + "{\n"; indent += "\t";
#define END_BLOCK(string) indent = indent.substr(0, indent.length() - 1); string += indent + "}\n";

///////////////////////////////////////////////////////////////////////////////
void exitConfig()
{
	EqualizerDisplaySystem* ds = (EqualizerDisplaySystem*)SystemManager::instance()->getDisplaySystem();
	ds->exitConfig();
}

///////////////////////////////////////////////////////////////////////////////
EqualizerDisplaySystem::EqualizerDisplaySystem():
	mySys(NULL),
	myConfig(NULL),
	myNodeFactory(NULL),
	mySetting(NULL),
	myDebugMouse(false)
{
}

///////////////////////////////////////////////////////////////////////////////
EqualizerDisplaySystem::~EqualizerDisplaySystem()
{
}
///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::exitConfig()
{
	SystemManager::instance()->postExitRequest();
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::generateEqConfig()
{
	DisplayConfig& eqcfg = myDisplayConfig;
	String indent = "";

	String result = L("#Equalizer 1.0 ascii");

	START_BLOCK(result, "global");

	result += 
		L("EQ_CONFIG_FATTR_EYE_BASE 0.06") +
		L("EQ_WINDOW_IATTR_PLANES_STENCIL ON");
		//L("EQ WINDOW IATTR HINT SWAPSYNC OFF");

	END_BLOCK(result);

	START_BLOCK(result, "server");

	START_BLOCK(result, "connection");
	result +=
		L("type TCPIP") +
		L(ostr("port %1%", %eqcfg.basePort));
	END_BLOCK(result);
	
	START_BLOCK(result, "config");
	// Latency > 0 makes everything explode when a local node is initialized, due to 
	// multiple shared data messages sent to slave nodes before they initialize their local objects
	result += L(ostr("latency %1%", %eqcfg.latency));

	for(int n = 0; n < eqcfg.numNodes; n++)
	{
		DisplayNodeConfig& nc = eqcfg.nodes[n];
		// If all tiles are disabled for this node, skip it.
		bool enabled = false;
		for(int i = 0; i < nc.numTiles; i++) enabled |= nc.tiles[i]->enabled;
		if(!enabled) continue;

		if(nc.isRemote)
		{
			int port = eqcfg.basePort + nc.port;
			START_BLOCK(result, "node");
			START_BLOCK(result, "connection");
			result +=
				L("type TCPIP") +
				L("hostname \"" + nc.hostname + "\"") +
				L(ostr("port %1%", %port));
			END_BLOCK(result);
			START_BLOCK(result, "attributes");
			result +=L("thread_model DRAW_SYNC");
			END_BLOCK(result);
		}
		else
		{
			START_BLOCK(result, "appNode");
			result += L("attributes { thread_model DRAW_SYNC }");
		}


		int winX = eqcfg.windowOffset[0];
		int winY = eqcfg.windowOffset[1];

		int curDevice = -1;

		// Write pipes section
		for(int i = 0; i < nc.numTiles; i++)
		{
			DisplayTileConfig& tc = *nc.tiles[i];
			if(tc.enabled)
			{
				winX = tc.position[0] + eqcfg.windowOffset[0];
				winY = tc.position[1] + eqcfg.windowOffset[1];
			
				String tileName = tc.name;
				String tileCfg = buildTileConfig(indent, tileName, winX, winY, tc.pixelSize[0], tc.pixelSize[1], tc.device, curDevice, eqcfg.fullscreen, tc.borderless);
				result += tileCfg;

				curDevice = tc.device;
			}
		}

		if(curDevice != -1)
		{		
			END_BLOCK(result); // End last open pipe section
		}

		// end of node
		END_BLOCK(result);
	}

	typedef pair<String, DisplayTileConfig*> TileIterator;

	// compounds
	START_BLOCK(result, "compound")
	foreach(TileIterator p, eqcfg.tiles)
	{
		DisplayTileConfig* tc = p.second;
		if(tc->enabled)
		{
			if(eqcfg.enableSwapSync)
			{
				//String tileCfg = ostr("\t\tcompound { swapbarrier { name \"defaultbarrier\" } channel ( canvas \"canvas-%1%\" segment \"segment-%2%\" layout \"layout-%3%\" view \"view-%4%\" ) }\n",
				String tileCfg = ostr("\t\tcompound { swapbarrier { name \"defaultbarrier\" } channel \"%1%\"\n",	%tc->name);
				START_BLOCK(tileCfg, "wall");
				tileCfg +=
					L("bottom_left [ -1 -0.5 0 ]") +
					L("bottom_right [ 1 -0.5 0 ]") +
					L("top_left [ -1 0.5 0 ]");
				END_BLOCK(tileCfg)
				result += tileCfg + "}\n";
			}
			else
			{
				String tileCfg = ostr("\t\tchannel \"%1%\"\n", %tc->name);
				START_BLOCK(tileCfg, "wall");
				tileCfg +=
					L("bottom_left [ -1 -0.5 0 ]") +
					L("bottom_right [ 1 -0.5 0 ]") +
					L("top_left [ -1 0.5 0 ]");
				END_BLOCK(tileCfg)
				result += tileCfg;
			}
		}
	}

	END_BLOCK(result)
	// ------------------------------------------ END compounds

	// end config
	END_BLOCK(result)

	// end server
	END_BLOCK(result)

	if(!eqcfg.disableConfigGenerator)
	{
		FILE* f = fopen(OMEGA_EQ_TMP_FILE, "w");
		fputs(result.c_str(), f);
		fclose(f);
	}
}

///////////////////////////////////////////////////////////////////////////////
String EqualizerDisplaySystem::buildTileConfig(String& indent, const String tileName, int x, int y, int width, int height, int device, int curdevice, bool fullscreen, bool borderless)
{
	String viewport = ostr("viewport [%1% %2% %3% %4%]", %x %y %width %height);

	String tileCfg = "";
	if(device != curdevice)
	{
		if(curdevice != -1) { END_BLOCK(tileCfg); } // End previous pipe section
		
		// Start new pipe section
		START_BLOCK(tileCfg, "pipe");
			tileCfg +=
				L(ostr("name = \"%1%-%2%\"", %tileName %device)) +
				L("port = 0") +
				L(ostr("device = %1%", %device));
	}
	START_BLOCK(tileCfg, "window");
	tileCfg +=
		L("name \"" + tileName + "\"") +
		L(viewport) +
		L("channel { name \"" + tileName + "\"}");
	if(fullscreen)
	{
		START_BLOCK(tileCfg, "attributes");
		tileCfg +=
			L("hint_fullscreen ON") +
			L("hint_decoration OFF");
		END_BLOCK(tileCfg);
	}
	else if(borderless)
	{
		START_BLOCK(tileCfg, "attributes");
		tileCfg +=
			L("hint_decoration OFF");
		END_BLOCK(tileCfg);
	}
	END_BLOCK(tileCfg)
	return tileCfg;
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::setup(Setting& scfg) 
{
	mySetting = &scfg;
	DisplayConfig::LoadConfig(scfg, myDisplayConfig);
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::setupEqInitArgs(int& numArgs, const char** argv)
{
	SystemManager* sys = SystemManager::instance();
	const char* appName = sys->getApplication()->getName();
	if(SystemManager::instance()->isMaster())
	{
		argv[0] = appName;
		argv[1] = "--eq-config";
		argv[2] = OMEGA_EQ_TMP_FILE;
		numArgs = 3;
	}
	else
	{
		argv[0] = appName;
		argv[1] = "--eq-client";
		argv[2] = "--eq-listen";
		argv[3] = SystemManager::instance()->getHostnameAndPort().c_str();
		numArgs = 4;
	}
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::initialize(SystemManager* sys)
{
#ifndef __APPLE__
	glewInit();
#endif
	if(getDisplayConfig().verbose) 	Log::level = LOG_INFO;
	else Log::level = LOG_WARN;
	mySys = sys;

	//atexit(::exitConfig);

	// Launch application instances on secondary nodes.
	if(SystemManager::instance()->isMaster())
	{
		// Generate the equalizer configuration
		generateEqConfig();
		
		for(int n = 0; n < myDisplayConfig.numNodes; n++)
		{
			DisplayNodeConfig& nc = myDisplayConfig.nodes[n];

			if(nc.hostname != "local")
			{
				// Launch the node if at least one of the tiles on the node is enabled.
				bool enabled = false;
				for(int i = 0; i < nc.numTiles; i++) enabled |= nc.tiles[i]->enabled;
				
				if(enabled)
				{
					String executable = StringUtils::replaceAll(myDisplayConfig.nodeLauncher, "%c", SystemManager::instance()->getApplication()->getExecutableName());
					executable = StringUtils::replaceAll(executable, "%h", nc.hostname);
				
					// Substitute %d with current working directory
					String cCurrentPath = ogetcwd();
					executable = StringUtils::replaceAll(executable, "%d", cCurrentPath);
				
					// Setup the executable call. Note: we pass a-D argument to tell all
					// instances what the main data directory is. We use ogetdataprefix
					// because omain sets the data prefix to the root data dir during
					// startup.
					int port = myDisplayConfig.basePort + nc.port;
					String cmd = ostr("%1% -c %2%@%3%:%4% -D %5%", %executable %SystemManager::instance()->getAppConfig()->getFilename() %nc.hostname %port %ogetdataprefix());
					olaunch(cmd);
				}
			}
		}
		osleep(myDisplayConfig.launcherInterval);
	}
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::killCluster() 
{
	omsg("EqualizerDisplaySystem::killCluster");
	if(SystemManager::instance()->isMaster())
	{
		ofmsg("number of nodes: %1%", %myDisplayConfig.numNodes);
		for(int n = 0; n < myDisplayConfig.numNodes; n++)
		{
			DisplayNodeConfig& nc = myDisplayConfig.nodes[n];

			if(nc.hostname != "local")
			{
				// Kill the node if at least one of the tiles on the node is enabled.
				bool enabled = false;
				for(int i = 0; i < nc.numTiles; i++) enabled |= nc.tiles[i]->enabled;
				if(enabled && myDisplayConfig.nodeKiller != "")
				{
					String executable = StringUtils::replaceAll(myDisplayConfig.nodeKiller, "%c", SystemManager::instance()->getApplication()->getName());
					executable = StringUtils::replaceAll(executable, "%h", nc.hostname);
					olaunch(executable);
				}
			}
		}
	}
	
	// kindof hack but it works: kill master instance.
	olaunch(ostr("killall %1%", %SystemManager::instance()->getApplication()->getName()));
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::finishInitialize(ConfigImpl* config, Engine* engine)
{
	myConfig = config;
	// Setup cameras for each tile.
	typedef KeyValue<String, DisplayTileConfig*> TileItem;
	foreach(TileItem dtc, myDisplayConfig.tiles)
	{
		if(dtc->cameraName == "")
		{
			// Use default camera for this tile
			dtc->camera = engine->getDefaultCamera();
		}
		else
		{
			// Use a custom camera for this tile (create it here if necessary)
			Camera* customCamera = engine->getCamera(dtc->cameraName);
			if(customCamera == NULL)
			{
				customCamera = engine->createCamera(dtc->cameraName);
			}
			dtc->camera = customCamera;
		}	
	}
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::run()
{
	bool error = false;
	const char* argv[4];
	int numArgs = 0;
	setupEqInitArgs(numArgs, (const char**)argv);
	myNodeFactory = new EqualizerNodeFactory();
	omsg(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> DISPLAY INITIALIZATION");
	if( !eq::init( numArgs, (char**)argv, myNodeFactory ))
	{
		oerror("Equalizer init failed");
	}
	
	myConfig = static_cast<ConfigImpl*>(eq::getConfig( numArgs, (char**)argv ));
	
	// If this is the master node, run the master loop.
	if(myConfig && mySys->isMaster())
	{
		//omsg(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> DISPLAY INITIALIZATION");
		if( myConfig->init())
		{
			omsg("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DISPLAY INITIALIZATION\n\n");
			omsg(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> APPLICATION LOOP");

			uint32_t spin = 0;
			bool exitRequestProcessed = false;
			while(!SystemManager::instance()->isExitRequested())
			{
				myConfig->startFrame( spin );
				myConfig->finishFrame();
				spin++;
				if(SystemManager::instance()->isExitRequested()
					&& !exitRequestProcessed)
				{
					exitRequestProcessed = true;

					// Run one additional frame, to give all omegalib objects
					// a change to dispose correctly.
					myConfig->startFrame( spin );
					myConfig->finishAllFrames();
				}
			}
			omsg("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< APPLICATION LOOP\n\n");
		}
		else
		{
			oerror("Config initialization failed!");
			error = true;
		}
	}
	else
	{
		oerror("Cannot get config");
		error = true;
	}    
}

///////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::cleanup()
{
	if(myConfig != NULL)
	{
		myConfig->exit();
		eq::releaseConfig( myConfig );
		eq::exit();
	}

	delete myNodeFactory;
	SharedDataServices::cleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EqualizerDisplaySystem::refreshSettings() 
{
	//myConfig->updateObserverCameras();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Vector2i EqualizerDisplaySystem::getCanvasSize()
{
	return myDisplayConfig.canvasPixelSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Ray EqualizerDisplaySystem::getViewRay(Vector2i position)
{
	int channelWidth = myDisplayConfig.tileResolution[0];
	int channelHeight = myDisplayConfig.tileResolution[1];
	int displayWidth = myDisplayConfig.canvasPixelSize[0];
	int displayHeight = myDisplayConfig.canvasPixelSize[1];

	//if(position[0] < 0 || position[0] > displayWidth || 
	//	position[1] < 0 || position[1] > displayHeight)
	//{
	//	ofwarn("EqualizerDisplaySystem::getViewRay: position out of bounds (%1%)", %position);
	//	return Ray();
	//}

	int channelX = position[0] / channelWidth;
	int channelY = position[1] / channelHeight;

	int x = position[0] % channelWidth;
	int y = position[1] % channelHeight;

	DisplayTileConfig* dtc = myDisplayConfig.tileGrid[channelX][channelY];
	if(dtc != NULL && !dtc->disableMouse)
	{
		// We found a tile in the grid that contains this mouse pointer event, and the tile mouse processing is active.
		return getViewRay(Vector2i(x, y), dtc);
	}

	// No luck using the grid: go with the slower but generic method. Loop through the tiles until you find one that
	// contains the pointer event.
	typedef pair<String, DisplayTileConfig*> TileItem;
	foreach(TileItem ti, myDisplayConfig.tiles)
	{
		if(!ti.second->disableMouse)
		{
			if(position[0] > ti.second->offset[0] &&
				position[1] > ti.second->offset[1] &&
				position[0] < ti.second->offset[0] + ti.second->pixelSize[0] &&
				position[1] < ti.second->offset[1] + ti.second->pixelSize[1])
			{
				Vector2i pos = position - ti.second->offset;
				return getViewRay(pos, ti.second);
			}
		}
	}

	// Suitable tile to process mouse pointer not found. return empty ray.
	return Ray();
}

///////////////////////////////////////////////////////////////////////////////
Ray EqualizerDisplaySystem::getViewRay(Vector2i position, DisplayTileConfig* dtc)
{
	int x = position[0];
	int y = position[1];

	// Try to use the camera attached to the tile first. If the camera is not set, switch to the default camera.
	Camera* camera = dtc->camera;
	if(camera == NULL)
	{
		camera = Engine::instance()->getDefaultCamera();
	}

	// If the camera is still null, we may be running this code during initialization (before full
	// tile data as been set up). Just print a warning and return an empty ray.
	if(camera == NULL)
	{
		owarn("EqualizerDisplaySystem::getViewRay: null camera, returning default ray.");
		return Ray();
	}

	Vector3f head = camera->getHeadOffset();

	float px = (float)x / dtc->pixelSize[0];
	float py = 1 - (float)y / dtc->pixelSize[1];

	Vector3f& vb = dtc->bottomLeft;
	Vector3f& va = dtc->topLeft;
	Vector3f& vc = dtc->bottomRight;

	Vector3f vba = va - vb;
	Vector3f vbc = vc - vb;

	Vector3f p = vb + vba * py + vbc * px;
	Vector3f direction = p - head;
	
	p = camera->getOrientation() * p;
	p += camera->getPosition();
	
	direction = camera->getOrientation() * direction;
	direction.normalize();

	//ofmsg("channel: %1%,%2% pixel:%3%,%4% pos: %5% dir %6%", %channelX %channelY %x %y %p %direction);

	return Ray(p, direction);
}

///////////////////////////////////////////////////////////////////////////////
bool EqualizerDisplaySystem::getViewRayFromEvent(const Event& evt, Ray& ray, bool normalizedPointerCoords)
{
	if(evt.getServiceType() == Service::Wand)
	{
		Camera* camera = Engine::instance()->getDefaultCamera();
		ray.setOrigin(camera->localToWorldPosition(evt.getPosition()));
		ray.setDirection(camera->localToWorldOrientation(evt.getOrientation()) * -Vector3f::UnitZ());

		return true;
	}
	else if(evt.getServiceType() == Service::Pointer)
	{
		// If the pointer already contains ray information just return it.
		if(evt.getExtraDataType() == Event::ExtraDataVector3Array &&
			evt.getExtraDataItems() >= 2)
		{
			ray.setOrigin(evt.getExtraDataVector3(0));
			ray.setDirection(evt.getExtraDataVector3(1));
		}
		else
		{
			Vector3f pos = evt.getPosition();
			// The pointer did not contain ray information: generate a ray now.
			if(normalizedPointerCoords)
			{
				pos[0] = pos[0] *  myDisplayConfig.canvasPixelSize[0];
				pos[1] = pos[1] *  myDisplayConfig.canvasPixelSize[1];
			}

			ray = getViewRay(Vector2i(pos[0], pos[1]));
		}
		return true;
	}
	return false;
}
