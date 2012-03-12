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
#include <osg/ShapeDrawable>


class TexEventHandler : public osgGA::GUIEventHandler
{
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
	osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);

	osgGeo::TexturePlaneNode* root = dynamic_cast<osgGeo::TexturePlaneNode*>( viewer->getSceneData() );
	if ( !root )
	{
	    osg::Group* group = dynamic_cast<osg::Group*>( viewer->getSceneData() );
	    if ( group )
		 root = dynamic_cast<osgGeo::TexturePlaneNode*>( group->getChild(0) );
	    if ( !root )
		return false;
	}

	if ( ea.getEventType() != osgGA::GUIEventAdapter::KEYDOWN )
	    return false;

	if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Up ||
	     ea.getKey()==osgGA::GUIEventAdapter::KEY_Down )
	{
	    osgGeo::LayeredTexture* tex = root->getLayeredTexture();
	    if ( !tex )
		return true;

	    if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Up )
	    {
		for ( int idx=0; idx<tex->nrProcesses(); idx++ )
		    tex->moveProcessLater( tex->getProcess(idx) );
		//tex->addProcess( tex->getProcess(0) );
		//tex->removeProcess( tex->getProcess(0) );
	    }
	    else
	    {
		for ( int idx=tex->nrProcesses(); idx>0; idx-- )
		    tex->moveProcessEarlier( tex->getProcess(idx) );
	    }

	    return true;
	}

	if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Right ||
	     ea.getKey()==osgGA::GUIEventAdapter::KEY_Left )
	{
	    int disperseFactor = root->getDisperseFactor();

	    if ( ea.getKey()==osgGA::GUIEventAdapter::KEY_Right )
		disperseFactor++;
	    else 
		disperseFactor--;

	    root->setDisperseFactor( disperseFactor );
	    return true;
	}

	return false;
    }
};


static unsigned char heatArray[1024];
static osgGeo::ColorSequence* heatColSeq = 0;

static osgGeo::ColorSequence* heatColorSequence()
{
    if ( !heatColSeq )
    {
	for ( int idx=0; idx<256; idx++ )
	{
	    heatArray[4*idx+0] = idx<128 ? 0 : 2*idx-255; 
	    heatArray[4*idx+1] = idx<128 ? 2*idx : 511-2*idx; 
	    heatArray[4*idx+2] = idx<128 ? 255-2*idx : 0;
	    heatArray[4*idx+3] = 255;
	}

	heatColSeq = new osgGeo::ColorSequence( heatArray );
    }
    return heatColSeq;
}


static unsigned char transparencyArray[1024];
static osgGeo::ColorSequence* transparencyColSeq = 0;

static osgGeo::ColorSequence* transparencyColorSequence()
{
    if ( !transparencyColSeq )
    {
	for ( int idx=0; idx<256; idx++ )
	{
	    transparencyArray[4*idx+0] = 255; 
	    transparencyArray[4*idx+1] = 255; 
	    transparencyArray[4*idx+2] = 255;
	    transparencyArray[4*idx+3] = idx<85 ? 0 : idx<170 ? 128 : 255;
	}

	transparencyColSeq = new osgGeo::ColorSequence( transparencyArray );
    }
    return transparencyColSeq;
}


void addColTabProcess( osgGeo::LayeredTexture& laytex, int id, float opac, int seqnr, int channel )
{
    osgGeo::ColorSequence* colSeq = heatColorSequence();
    if ( seqnr%2 )
	colSeq = transparencyColorSequence();

    osg::ref_ptr<osgGeo::ColTabLayerProcess> process = new osgGeo::ColTabLayerProcess();
    process->setDataLayerID( id, channel-1 );
    process->setDataLayerColorSequence( colSeq );
    process->setOpacity( opac );
    laytex.addProcess( process );
}


void addRGBAProcess( osgGeo::LayeredTexture& laytex, int id, float opac, int r, int g, int b, int a )
{
    osg::ref_ptr<osgGeo::RGBALayerProcess> process = new osgGeo::RGBALayerProcess();

    if ( r ) process->setDataLayerID( 0, id, r-1 );
    if ( g ) process->setDataLayerID( 1, id, g-1 );
    if ( b ) process->setDataLayerID( 2, id, b-1 );
    if ( a ) process->setDataLayerID( 3, id, a-1 );

    process->setOpacity( opac );
    laytex.addProcess( process );
}
	    

int main( int argc, char** argv )
{
    osg::ArgumentParser args( &argc, argv );

    osg::ApplicationUsage* usage = args.getApplicationUsage();
    usage->setCommandLineUsage( "textureplane [options]" );
    usage->setDescription( "3D view of tiled plane with layered set of textures or one default texture" );
    usage->addCommandLineOption( "--bricksize <n>", "Desired brick size" );
    usage->addCommandLineOption( "--dim <n>", "Thin dimension [0,2]" );
    usage->addCommandLineOption( "--help | --usage", "Command line info" );
    usage->addCommandLineOption( "--image <path> [origin-opt] [scale-opt] [opacity-opt] [colormap-opt] [rgbamap-opt]", "Add texture layer" );
    usage->addCommandLineOption( "--origin <x0> <y0>", "Layer origin" );
    usage->addCommandLineOption( "--scale <dx> <dy>", "Layer scale" );
    usage->addCommandLineOption( "--opacity <frac> ", "Layer opacity [0.0,1.0]" );
    usage->addCommandLineOption( "--colormap <n> <channel>", "Color map <n>  from channel [1,4]" );
    usage->addCommandLineOption( "--rgbamap <r> <g> <b> <a>", "RGBA map from channels [0=void,4]" );
    usage->addKeyboardMouseBinding( "Left/Right arrow", "Disperse tiles" );
    usage->addKeyboardMouseBinding( "Up/Down arrow", "Rotate layers" );

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
	    args.reportError( "Thin dimension not in [0,2]" );
	    thinDim = 1;
	}
    }

    while ( args.read("--bricksize", brickSize) )
    {
	if ( brickSize < 2 )
	{
	    args.reportError( "Brick size must be at least 2" );
	    brickSize = 2;
	}
    }

    osg::ref_ptr<osgGeo::LayeredTexture> laytex = new osgGeo::LayeredTexture();
    int lastId = laytex->addDataLayer();
    int pos = 0;
    float opacity = 1.0;
    int channel = -1;
    int seqnr = 0;
    int r, g, b, a; 
    r = g = b = a = -1;

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

	    if ( channel>=0 )
		addColTabProcess( *laytex, lastId, opacity, seqnr, channel );
	    else if ( r>=0 || g>=0 || b>=0 || a>=0 )
		addRGBAProcess( *laytex, lastId, opacity, r, g, b, a );
	    else
	    {
		osg::ref_ptr<osgGeo::IdentityLayerProcess> process = new osgGeo::IdentityLayerProcess( lastId );
		process->setOpacity( opacity );
		laytex->addProcess( process );
	    }

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

	if ( args.read(pos, "--opacity", opacity) )
	{
	    if ( opacity<0.0f || opacity>1.0f )
		args.reportError( "Opacity not in [0.0,1.0]" );

	    const int nrProc = laytex->nrProcesses();
	    if ( nrProc )
	    {
		laytex->getProcess(nrProc-1)->setOpacity( opacity );
		opacity = 1.0;
	    }
	    continue;
	}

	if ( args.read(pos, "--colormap", seqnr, channel) )
	{
	    if ( channel<1 || channel>4 )
		args.reportError( "Channel not in [1,4]" );

	    r = g = b = a = -1;
	    const int nrProc = laytex->nrProcesses();
	    if ( nrProc )
	    {
		const float opac = laytex->getProcess(nrProc-1)->getOpacity();
		laytex->removeProcess( laytex->getProcess(nrProc-1) );
		addColTabProcess( *laytex, lastId, opac, seqnr, channel );
		channel = -1;
	    }
	    continue;
	}

	if ( args.read(pos, "--rgbamap", r, g, b, a) )
	{
	    if ( r<0 || r>4 || g<0 || g>4 || b<0 || b>4 || a<0 || a>4 )
		args.reportError( "Channel not in [0=void,4]" );

	    channel = -1;
	    const int nrProc = laytex->nrProcesses();
	    if ( nrProc )
	    {
		const float opac = laytex->getProcess(nrProc-1)->getOpacity();
		laytex->removeProcess( laytex->getProcess(nrProc-1) );
		addRGBAProcess( *laytex, lastId, opac, r, g, b, a );
		r = g = b = a = -1;
	    }
	    continue;
	}

	pos++;
    }

    args.reportRemainingOptionsAsUnrecognized(); 
    args.writeErrorMessages( std::cerr );

    osg::ref_ptr<osgGeo::TexturePlaneNode> root = new osgGeo::TexturePlaneNode();
    root->setLayeredTexture( laytex );

    // Fit to screen
    const osg::Vec2f envelopeSize = laytex->envelopeSize();
    osg::Vec3 center( laytex->envelopeCenter()/envelopeSize.x(), 0.0f );
    osg::Vec3 width( envelopeSize/envelopeSize.x(), 0.0f );
    if ( width.y() < 1.0f )
    {
	center = osg::Vec3( laytex->envelopeCenter()/envelopeSize.y(), 0.0f );
	width = osg::Vec3( envelopeSize/envelopeSize.y(), 0.0f );
    }

    if ( thinDim==0 )
    {
	width = osg::Vec3( 0.0f, width.x(), width.y() );
	center = osg::Vec3( 0.0f, center.x(), center.y() );
    }
    else if ( thinDim==1 )
    {
	width = osg::Vec3( width.x(), 0.0f, width.y() );
	center = osg::Vec3( center.x(), 0.0f, center.y() );
    }

    width.x() *= -1;		// Mirror x-dimension
    center.x() *= -1;

    //root->setCenter( center );  // Move texture origin to center of screen
    root->setWidth( width );
    root->setTextureBrickSize( brickSize );

    osg::ref_ptr<osg::Node> model = osgDB::readNodeFile( "cessna.osg" );

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::ShapeDrawable> sphere = new osg::ShapeDrawable;
    sphere->setShape( new osg::Sphere(osg::Vec3(0.0f,2.0f,0.0f),1.0f) );
    sphere->setColor( osg::Vec4(1.0f,0.0f,1.0f,1.0f) );
    geode->addDrawable( sphere.get() );

    osg::ref_ptr<osg::ShapeDrawable> cone = new osg::ShapeDrawable;
    cone->setShape( new osg::Cone(osg::Vec3(0.0f,-2.0f,-2.0f),1.0f,1.0f) );
    geode->addDrawable( cone.get() );


    osg::ref_ptr<osg::Group> group = new osg::Group;
    group->addChild( root.get() );
    group->addChild( geode.get() );

    osgViewer::Viewer viewer;
    viewer.setSceneData( group.get() );

    TexEventHandler* texEventHandler = new TexEventHandler;
    viewer.addEventHandler( texEventHandler );

    return viewer.run();
}
