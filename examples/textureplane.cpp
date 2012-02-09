/* osgGeo - A collection of geoscientific extensions to OpenSceneGraph.
Copyright 2011 dGB Beheer B.V.

osgGeo is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include <osgGeo/LayeredTexture>
#include <osgGeo/TexturePlane>
#include <osgViewer/Viewer>
#include <osgDB/ReadFile>
#include <osgUtil/UpdateVisitor>


class TexEventHandler : public osgGA::GUIEventHandler
{
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
	osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
	osgGeo::TexturePlaneNode* root = dynamic_cast<osgGeo::TexturePlaneNode*>( viewer->getSceneData() );
	if ( !root )
	    return false;

	if ( ea.getEventType() != osgGA::GUIEventAdapter::KEYDOWN )
	    return false;

	int textureUnit = root->selectedTextureUnit();
	if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Up ||
	     ea.getKey()==osgGA::GUIEventAdapter::KEY_Down )
	{
	    if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Up )
		textureUnit++;
	    else if ( textureUnit >= 0 )
		textureUnit--;

	    root->selectTextureUnit( textureUnit );
	    return true;
	}

	int disperseFactor = root->getDisperseFactor();
	if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Right ||
	     ea.getKey()==osgGA::GUIEventAdapter::KEY_Left )
	{
	    if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Right )
		disperseFactor++;
	    else 
		disperseFactor--;

	    root->setDisperseFactor( disperseFactor );

	    osgUtil::UpdateVisitor updateVisitor;
	    root->traverse( updateVisitor ); 
	    return true;
	}



	return false;
    }
};


int main( int argc, char** argv )
{
    osg::ArgumentParser args( &argc, argv );

    osg::ApplicationUsage* usage = args.getApplicationUsage();
    usage->setCommandLineUsage( "textureplane [options]" );
    usage->setDescription( "3D view of tiled plane with layered set of textures or one default texture" );
    usage->addCommandLineOption( "--bricksize <n>", "Desired brick size" );
    usage->addCommandLineOption( "--dim <n>", "Thin dimension {0,1,2}" );
    usage->addCommandLineOption( "--help | --usage", "Command line info" );
    usage->addCommandLineOption( "--image <path> [origin-option] [scale-option]", "Add texture layer" );
    usage->addCommandLineOption( "--origin <x0> <y0>", "Layer origin" );
    usage->addCommandLineOption( "--scale <dx> <dy>", "Layer scale" );
    usage->addKeyboardMouseBinding( "Left/Right arrow", "Disperse tiles" );
    usage->addKeyboardMouseBinding( "Up/Down arrow", "Select texture unit(s)" );

    if ( args.read("--help") || args.read("--usage") )
    {
	std::cout << std::endl << usage->getDescription() << std::endl << std::endl;
	usage->write( std::cout );
	usage->write( std::cout, usage->getKeyboardMouseBindings() );
	return 1;
    }

    int thinDim = 1;
    int brickSize = 64;
    bool disperseTiles = false;

    while ( args.read("--dim", thinDim) )
    {
	if ( thinDim<0 || thinDim>2 )
	{
	    args.reportError( "Thin dimension not in {0,1,2}" );
	    thinDim = 1;
	}
    }

    while ( args.read("--bricksize", brickSize) )
    {
	if ( brickSize < 2 )
	    args.reportError( "Brick size must be at least 2" );
    }

    osg::ref_ptr<osgGeo::LayeredTexture> laytex = new osgGeo::LayeredTexture();
    int lastId = laytex->addDataLayer();
    int pos = 0;

    while ( pos <= args.argc() )
    {
	std::string imagePath;
	if ( pos>=args.argc() && !laytex->getDataLayerImage(lastId) )
	    imagePath = "Images/dog_left_eye.jpg";
	else
	    args.read( pos, "--image", imagePath );

	if ( !imagePath.empty() )
	{
	    osg::ref_ptr<osg::Image> img = osgDB::readImageFile( imagePath );
	    if ( !img || !img->s() || !img->t() )
	    {
		args.reportError( "Invalid texture image: " + imagePath );
		continue;
	    }

	    if ( laytex->getDataLayerImage(lastId) )
		lastId = laytex->addDataLayer();

	    laytex->setDataLayerOrigin( lastId, osg::Vec2f(0.0f,0.0f) );
	    laytex->setDataLayerScale( lastId, osg::Vec2f(1.0f,1.0f) );
	    laytex->setDataLayerImage( lastId, img );
	    laytex->setDataLayerTextureUnit( lastId, lastId );
	    continue;
	}

	osg::Vec2f origin;
	if ( args.read(pos, "--origin", origin.x(), origin.y()) )
	{
	    laytex->setDataLayerOrigin(lastId, origin );
	    continue;
	}

	osg::Vec2f scale;
	if ( args.read(pos, "--scale", scale.x(), scale.y()) )
	{
	    if ( scale.x()<=0.0f || scale.y()<=0.0f )
		args.reportError( "Scales have to be positive" );

	    laytex->setDataLayerScale( lastId, scale );
	    continue;
	}

	pos++;
    }

    args.reportRemainingOptionsAsUnrecognized(); 
    args.writeErrorMessages( std::cerr );

    osg::ref_ptr<osgGeo::TexturePlaneNode> root = new osgGeo::TexturePlaneNode();
    root->setLayeredTexture( laytex );
    
    const osg::Vec2f envelope = laytex->calculateEnvelope();
    osg::Vec3 width( 1.0, float(envelope.y()) / float(envelope.x()), 0.0f );
    if ( width.y() < 1.0f )
	width = osg::Vec3( float(envelope.x()) / float(envelope.y()), 1.0f, 0.0f );

    if ( thinDim==0 )
	width = osg::Vec3( 0.0f, width.x(), width.y() );
    else if ( thinDim==1 )
	width = osg::Vec3( width.x(), 0.0f, width.y() );

    root->setWidth( width );
    root->setTextureBrickSize( brickSize );

    osgUtil::UpdateVisitor updateVisitor;
    root->traverse( updateVisitor ); 

    osgViewer::Viewer viewer;
    viewer.setSceneData( root.get() );

    TexEventHandler* texEventHandler = new TexEventHandler;
    viewer.addEventHandler( texEventHandler );

    return viewer.run();
}
