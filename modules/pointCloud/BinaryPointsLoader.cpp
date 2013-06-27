#include "BinaryPointsLoader.h"

#include <osg/Geode>
#include <osg/Point>

using namespace omega;
using namespace cyclops;

///////////////////////////////////////////////////////////////////////////////
BinaryPointsLoader::BinaryPointsLoader(): ModelLoader("points-binary")
{
}

///////////////////////////////////////////////////////////////////////////////
BinaryPointsLoader::~BinaryPointsLoader()
{
}

///////////////////////////////////////////////////////////////////////////////
bool BinaryPointsLoader::supportsExtension(const String& ext) 
{ 
	if(ext == "xyzb") return true;
	return false; 
}

///////////////////////////////////////////////////////////////////////////////
bool BinaryPointsLoader::load(ModelAsset* model)
{
    osg::ref_ptr<osg::Group> group = new osg::Group();

	bool result = loadFile(model->info->path, model->info->options, group);

    // if successful get last child and add to sceneobject
    if(result)
    {
		osg::Geode* points;
	    points = group->getChild(0)->asGeode();
	    
		model->nodes.push_back(points);

	    group->removeChild(0, 1);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////
bool BinaryPointsLoader::loadFile(
	const String& filename, const String& options, osg::Group * grp)
{
	if(!grp)
	{
		return false;
	}

  	osg::Vec3Array* verticesP = new osg::Vec3Array();
  	osg::Vec4Array* verticesC = new osg::Vec4Array();

	String path;
	if(DataManager::findFile(filename, path))
	{ 
		readXYZ(path, options, verticesP, verticesC);

  		// create geometry and geodes to hold the data
  		osg::Geode* geode = new osg::Geode();
  		geode->setCullingActive(false);
  		osg::Geometry* nodeGeom = new osg::Geometry();
  		osg::StateSet *state = nodeGeom->getOrCreateStateSet();
		nodeGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS,0, verticesP->size()));
  		osg::VertexBufferObject* vboP = nodeGeom->getOrCreateVertexBufferObject();
  		vboP->setUsage (GL_STREAM_DRAW);

  		nodeGeom->setUseDisplayList (false);
  		nodeGeom->setUseVertexBufferObjects(true);
  		nodeGeom->setVertexArray(verticesP);
  		nodeGeom->setColorArray(verticesC);
  		nodeGeom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  		geode->addDrawable(nodeGeom);
  		geode->dirtyBound();
 
		grp->addChild(geode);
		return true;
	}
	return false; 
}

///////////////////////////////////////////////////////////////////////////////
void BinaryPointsLoader::readXYZ(
	const String& filename, const String& options, 
	osg::Vec3Array* points, osg::Vec4Array* colors)
{
	osg::Vec3f point;
	osg::Vec4f color(1.0f, 1.0f, 1.0f, 1.0f);

	// Default record size = 7 doubles (X,Y,Z,R,G,B,A)
	size_t recordSize = sizeof(double) * 7;

	// Check the options to see if we should read a subsection of the file
	// and / or use decimation.
	int readStart = 0;
	int readLength = 0;
	int decimation = 0;

	if(options != "")
	{
		// Only format supported is XYZRGBA
		if(StringUtils::startsWith(options, "xyzrgba"))
		{
			recordSize = sizeof(double) * 7;
		}
		else
		{
			ofwarn("BinaryPointsLoader::readXYZ: invalid file format specified in options. \n   "
				"Options string should start with xyzrgba (is %1%)", %options);
			return;
		}

		libconfig::ArgumentHelper ah;
		ah.newNamedInt('s', "start", "start", "start record", readStart);
		ah.newNamedInt('l', "length", "length", "number of records to read", readLength);
		ah.newNamedInt('d', "decimation", "decimation", "read decimation", decimation);
		ah.process(options.c_str());
	}

	FILE* fin = fopen(filename.c_str(), "rb");

	// How many records are in the file?
	fseek(fin, 0, SEEK_END);
	long endpos = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	int numRecords = endpos / recordSize;

	if(readStart != 0)
	{
		fseek(fin, readStart * recordSize, SEEK_SET);
	}
	// Adjust read length.
	if(readLength == 0 || readStart + readLength > numRecords)
	{
		readLength = numRecords - readStart;
	}

	// Read in data
	double* buffer = (double*)malloc(recordSize * readLength);
	if(buffer == NULL)
	{
		oferror("BinaryPointsLoader::readXYZ: could not allocate %1% bytes", 
			%(recordSize * readLength));
		return;
	}

	fread(buffer, recordSize, readLength, fin);

	if(decimation <= 0) decimation = 1;
	for(int i = 0; i < readLength; i += decimation)
	{
		points->push_back(osg::Vec3f(
			buffer[i * recordSize],
			buffer[i * recordSize + 1],
			buffer[i * recordSize + 2]));
		colors->push_back(osg::Vec4f(
			buffer[i * recordSize + 3],
			buffer[i * recordSize + 4],
			buffer[i * recordSize + 5],
			buffer[i * recordSize + 6]));
	}
	
	fclose(fin);
	free(buffer);
}
