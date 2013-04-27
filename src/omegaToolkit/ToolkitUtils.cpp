/**************************************************************************************************
 * THE OMEGA LIB PROJECT
 *-------------------------------------------------------------------------------------------------
 * Copyright 2010-2013		Electronic Visualization Laboratory, University of Illinois at Chicago
 * Authors:										
 *  Alessandro Febretti		febret@gmail.com
 *-------------------------------------------------------------------------------------------------
 * Copyright (c) 2010-2013, Electronic Visualization Laboratory, University of Illinois at Chicago
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted 
 * provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list of conditions 
 * and the following disclaimer. Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the documentation and/or other 
 * materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF 
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************************************/
#include "omegaToolkit/ToolkitUtils.h"
#include "omegaToolkit/MouseManipulator.h"
#include "omegaToolkit/DefaultMouseInteractor.h"
#include "omegaToolkit/ControllerManipulator.h"
#include "omegaToolkit/DefaultTwoHandsInteractor.h"
#include "omegaToolkit/WandManipulator.h"

using namespace omega;
using namespace omegaToolkit;

#ifdef OMICRON_USE_OPENNI
	#include "omicron/OpenNIService.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
Actor* ToolkitUtils::createInteractor(const Setting& s)
{
	String interactorStyle = Config::getStringValue("style", s);
	StringUtils::toLowerCase(interactorStyle);
	ofmsg("Creating interactor of type %1%:", %interactorStyle);
	if(interactorStyle == "")
	{
		return NULL;
	}
    else if(interactorStyle == "mouse")
    {
        DefaultMouseInteractor* interactor = new DefaultMouseInteractor();
        interactor->setMoveButtonFlag(Event::Left);
        interactor->setRotateButtonFlag(Event::Right);
        return interactor;
    }
    else if(interactorStyle == "simpleMouse")
    {
        MouseManipulator* interactor = new MouseManipulator();
        interactor->setMoveButtonFlag(Event::Left);
        interactor->setRotateButtonFlag(Event::Right);
        return interactor;
    }
    else if(interactorStyle == "controller")
    {
        ControllerManipulator* interactor = new ControllerManipulator();
        return interactor;
    }
    else if(interactorStyle == "wand")
    {
        WandManipulator* interactor = new WandManipulator();
		interactor->setup(s);
        return interactor;
    }
    else if(interactorStyle == "twohands")
    {
        DefaultTwoHandsInteractor* interactor =  new DefaultTwoHandsInteractor();
        interactor->initialize("ObserverUpdateService");
        return interactor;
    }
	ofwarn("Unknown interactor style: %1%", %interactorStyle);
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
Actor* ToolkitUtils::setupInteractor(const String& settingName)
{
	Actor* interactor = NULL;

    // Set the interactor style used to manipulate meshes.
	if(SystemManager::settingExists(settingName))
	{
		Setting& sinteractor = SystemManager::settingLookup(settingName);
		interactor = ToolkitUtils::createInteractor(sinteractor);
		if(interactor != NULL)
		{
			ModuleServices::addModule(interactor);
		}
	}
	return interactor;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
PixelData* ToolkitUtils::getKinectDepthCameraImage(const String& kinectServiceName)
{
#ifdef OMICRON_USE_OPENNI
	OpenNIService* svc = SystemManager::instance()->getServiceManager()->findService<OpenNIService>(kinectServiceName);
	return new PixelData(PixelData::FormatRgb, svc->getImageDataWidth(), svc->getImageDataHeight(), (byte*)svc->getDepthImageData());
#else
	return NULL;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ToolkitUtils::centerChildren(SceneNode* node)
{
	AlignedBox3 bbox;
	foreach(Node* c, node->getChildren())
	{
		SceneNode* child = dynamic_cast<SceneNode*>(c);
		if(child != NULL)
		{
			bbox.merge(child->getBoundingBox());
		}
	}

	Vector3f offset = bbox.getCenter();
	node->translate(-offset);
}

