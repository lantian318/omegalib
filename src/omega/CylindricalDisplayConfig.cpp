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
 *	A display configuration builder that can be used for cylindrical display
 *  systems like CAVE2.
 ******************************************************************************/
#include "omega/CylindricalDisplayConfig.h"

using namespace omega;

///////////////////////////////////////////////////////////////////////////////
bool CylindricalDisplayConfig::buildConfig(DisplayConfig& cfg, Setting& scfg)
{
    Vector2i numTiles = Config::getVector2iValue("numTiles", scfg);
    cfg.tileGridSize = numTiles;

    cfg.canvasPixelSize = numTiles.cwiseProduct(cfg.tileResolution);
    ofmsg("canvas pixel size: %1%", %cfg.canvasPixelSize);

    int numSides = numTiles.x();
    
    // Angle increment for each side (column)
    float sideAngleIncrement = Config::getFloatValue("sideAngleIncrement", scfg, 90);

    // Angle of side containing 0-index tiles.
    float sideAngleStart = Config::getFloatValue("sideAngleStart", scfg, -90);

    // Display system cylinder radius
    myRadius = Config::getFloatValue("radius", scfg, 5);

    // Number of vertical tiles in each side
    int numSideTiles = numTiles.y();

    // Offset of center of bottom tile.
    float yOffset = cfg.referenceOffset.y();

    // Save offset of low end of bottom tile (subtract center)
    myYOffset = yOffset - cfg.tileSize.y() / 2;

    // Save cylinder height
    myHeight = numSideTiles * cfg.tileSize.y();

    float tileViewportWidth = 1.0f / numTiles[0];
    float tileViewportHeight = 1.0f / numTiles[1];
    float tileViewportX = 0.0f;
    float tileViewportY = 0.0f;

    // Fill up the tile position / orientation data.
    // Compute the edge coordinates for all sides
    float curAngle = sideAngleStart;
    for(int x = 0; x < numSides; x ++)
    {
        float yPos = yOffset;
        for(int y = 0; y < numSideTiles; y ++)
        {
            // Use the indices to create a tile name in the form t<X>x<Y> (i.e. t1x0).
            // This is kind of hacking, because it forces tile names to be in that form for cylindrical configurations, 
            // but it works well enough.
            String tileName = ostr("t%1%x%2%", %x %y);
            if(cfg.tiles.find(tileName) == cfg.tiles.end())
            {
                ofwarn("CylindricalDisplayConfig::buildConfig: could not find tile '%1%'", %tileName);
            }
            else
            {
                DisplayTileConfig* tc = cfg.tiles[tileName];
                cfg.tileGrid[x][y] = tc;
                
                tc->enabled = true;
                tc->isInGrid = true;
                tc->gridX = x;
                tc->gridY = y;
                
                tc->yaw = curAngle;
                tc->pitch = 0;
                tc->center = Vector3f(
                    sin(curAngle * Math::DegToRad) * myRadius,
                    yPos,
                    -1 * cos(curAngle * Math::DegToRad) * myRadius);

                // Save the tile viewport
                //tc->viewport = Vector4f(tileViewportX, tileViewportY, tileViewportWidth, tileViewportHeight);
                
                // Compute this tile pixel offset.
                // Note that the tile row index is inverted wrt. pixel coordinates 
                // (tile row 0 is at the bottom of the cylinder, while pixel 
                // row 0 is at the top). We take this into account to compute
                // correct pixel offsets for each tile.
                tc->offset[0] = tc->pixelSize[0] * x;
                tc->offset[1] = tc->pixelSize[1] * (numSideTiles - y - 1);

                tc->computeTileCorners();
            }
            yPos += cfg.tileSize.y();
            tileViewportY += tileViewportHeight;
        }
        curAngle += sideAngleIncrement;
        tileViewportY = 0.0f;
        tileViewportX += tileViewportWidth;
    }

    // Register myself as a ray-to-point converter: code needing to convert view
    // rays (like the ones generated by wand events) to display pointers for
    // 2D interaction will use this class getPointFromRay method.
    // The function is optimized for cylindrical displays and is much simpler
    // than a generic display config ray-to-point function.
    cfg.rayToPointConverter = this;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
std::pair<bool, Vector2f> CylindricalDisplayConfig::calculateScreenPosition(float x, float y, float z)
{
    typedef std::pair<bool, Vector2f> Result;
    float MAX_Y_ERROR = 0.5f;
    float MAX_X_ERROR = 0.05f;

    float MAX_Y = myHeight + myYOffset;
    float MIN_Y = myYOffset;

    // NOTE: this should come from the config file
    float RADIANS_FOR_DOOR = 36 * Math::Pi / 180;

    if(y > MAX_Y)
    {
        if(y < MAX_Y + MAX_Y_ERROR) y = MAX_Y;
        else return Result(false, Vector2f::Zero());
    }
    if(y < MIN_Y)
    {
        if(y > MAX_Y - MAX_Y_ERROR) y = MIN_Y;
        else return Result(false, Vector2f::Zero());
    }

    float angle = atan2(x, z);
    if(angle < 0)
    {
        angle += Math::TwoPi;
    }
    angle -= 2*Math::TwoPi;
    angle -= RADIANS_FOR_DOOR / 2;

    x = angle / (Math::TwoPi - RADIANS_FOR_DOOR);

    // Marrinan offset correction
    x += 0.027777f;

    if(x > 1)
    {
        if(x < 1 + MAX_X_ERROR) x = 1;
        else return Result(false, Vector2f::Zero());
    }
    if(x < 0)
    {
        if(x > -MAX_X_ERROR) x = 0;
        else return Result(false, Vector2f::Zero());
    }

    y -= MIN_Y;
    y /= (MAX_Y - MIN_Y);
    return Result(true, Vector2f(x, 1-y));
}

///////////////////////////////////////////////////////////////////////////////
std::pair<bool, Vector2f> CylindricalDisplayConfig::getPointFromRay(const Ray& ray)
{
    typedef std::pair<bool, Vector2f> Result;

    float ox = ray.getDirection().x();
    float oy = ray.getDirection().x();
    float oz = ray.getDirection().x();
    float x0 = ray.getOrigin().x();
    float y0 = ray.getOrigin().y();
    float z0 = ray.getOrigin().z();
    float r = myRadius;

    // x coordinate of center of circle
    float h = 0.0f;
    // z coordinate of center of circle
    float k = 0.0f;

    float a = ox*ox + oz*oz;
    float b = 2*ox*x0 + 2*oz*z0 - 2*h*ox - 2*k*oz;
    float c = x0*x0 + z0*z0 + h*h + k*k - r*r - 2*h*x0 - 2*k*z0;

    if(a != 0 && (b*b - 4*a*c) >= 0)
    {
        float t1 = (-b + sqrt(b*b - 4*a*c)) / (2*a);
        float t2 = (-b - sqrt(b*b - 4*a*c)) / (2*a);
        float t = 0;
        if(t1 >= 0) t = t1;
        else if(t2 >= 0) t = t2;
        else
        {
            return Result(false, Vector2f::Zero());
        }
        float xpos = ox*t + x0;
        float ypos = oy*t + y0;
        float zpos = oz*t + z0;
        return calculateScreenPosition(xpos, ypos, zpos);
    }
    return Result(false, Vector2f::Zero());
}
