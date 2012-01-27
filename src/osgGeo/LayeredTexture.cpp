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

#include <osg/Geometry>
#include <osg/Texture2D>
#include <osgUtil/CullVisitor>

#include <string.h>


namespace osgGeo
{

struct LayeredTextureData : public osg::Referenced
{
			LayeredTextureData(int id)
			    : _id( id )
			    , _origin( 0, 0 )
			    , _scale( 1, 1 )
			    , _updateSetupStateSet( true )
			    , _textureUnit( 0 )
			    , _image( 0 )
			{}

    LayeredTextureData*	clone() const;
    osg::Vec2f		getLayerCoord(const osgGeo::Vec2i& global) const;

    TransparencyType	getTransparencyType();

    const int				_id;
    osg::Vec2f				_origin;
    osg::Vec2f				_scale;
    osg::ref_ptr<const osg::Image>	_image;
    bool				_updateSetupStateSet;
    int					_textureUnit;
};


LayeredTextureData* LayeredTextureData::clone() const
{
    LayeredTextureData* res = new LayeredTextureData( _id );
    res->_origin = _origin;
    res->_scale = _scale; 
    res->_updateSetupStateSet = _updateSetupStateSet; 
    res->_textureUnit = _textureUnit;
    if ( _image )
	res->_image = (osg::Image*) _image->clone(osg::CopyOp::DEEP_COPY_ALL);

    return res;
}


osg::Vec2f LayeredTextureData::getLayerCoord( const osgGeo::Vec2i& global ) const
{
    osg::Vec2f res = osg::Vec2f(global._v[0], global._v[1] )-_origin;
    res.x() /= _scale.x();
    res.y() /= _scale.y();
    
    return res;
}


LayeredTexture::LayeredTexture()
    : _freeId( 0 )
    , _updateSetupStateSet( false )
{}


LayeredTexture::LayeredTexture( const LayeredTexture& lt,
				const osg::CopyOp& co )
    : osg::Object( lt, co )
    , _freeId( lt._freeId )
    , _updateSetupStateSet( false )
    , _setupStateSet( 0 )
{
    for ( int idx=0; idx<lt._dataLayers.size(); idx++ )
    {
	osg::ref_ptr<LayeredTextureData> layer =
		co.getCopyFlags()==osg::CopyOp::DEEP_COPY_ALL
	    ? lt._dataLayers[idx]->clone()
	    : lt._dataLayers[idx];

	layer->ref();
	_dataLayers.push_back( layer );
    }
}


LayeredTexture::~LayeredTexture()
{
    std::for_each( _dataLayers.begin(), _dataLayers.end(),
	    	   osg::intrusive_ptr_release );

    std::for_each( _processes.begin(), _processes.end(),
	    	   osg::intrusive_ptr_release );
}


int LayeredTexture::addDataLayer()
{
    _lock.writeLock();
    osg::ref_ptr<LayeredTextureData> ltd = new LayeredTextureData( _freeId++ );
    if ( ltd )
    {
	ltd->ref();
	_dataLayers.push_back( ltd );
    }

    _updateSetupStateSet = true;

    _lock.writeUnlock();
    return ltd ? ltd->_id : -1;
}


void LayeredTexture::removeDataLayer( int id )
{
    _lock.writeLock();
    const int idx = getDataLayerIndex( id );
    if ( idx!=-1 )
    {
	osg::ref_ptr<LayeredTextureData> ltd = _dataLayers[idx];
	_dataLayers.erase( _dataLayers.begin()+idx );
	_updateSetupStateSet = true;
	ltd->unref();
    }

    _lock.writeUnlock();
}


int LayeredTexture::getDataLayerID( int idx ) const
{
    return idx>=0 && idx<_dataLayers.size() 
	? _dataLayers[idx]->_id
	: -1;
}


int LayeredTexture::getDataLayerIndex( int id ) const
{
    for ( int idx=_dataLayers.size()-1; idx>=0; idx-- )
    {
	if ( _dataLayers[idx]->_id==id )
	    return idx;
    }

    return -1;
}

#define SET_GET_PROP( funcpostfix, type, variable ) \
void LayeredTexture::setDataLayer##funcpostfix( int id, type localvar ) \
{ \
    const int idx = getDataLayerIndex( id ); \
    if ( idx!=-1 ) \
	_dataLayers[idx]->variable = localvar; \
} \
 \
 \
type LayeredTexture::getDataLayer##funcpostfix( int id ) const \
{ \
    const int idx = getDataLayerIndex( id ); \
    return _dataLayers[idx]->variable; \
}

SET_GET_PROP( Image, const osg::Image*, _image )
SET_GET_PROP( Origin, const osg::Vec2f&, _origin )
SET_GET_PROP( Scale, const osg::Vec2f&, _scale )
SET_GET_PROP( TextureUnit, int, _textureUnit )

void LayeredTexture::addProcess( LayerProcess* process )
{
    process->ref();
    _lock.writeLock();
    _processes.push_back( process );
    _updateSetupStateSet = true;
    _lock.writeUnlock();
}


void LayeredTexture::removeProcess( const LayerProcess* process )
{
    _lock.writeLock();
    std::vector<LayerProcess*>::iterator it = std::find( _processes.begin(),
	    					    _processes.end(), process );
    if ( it!=_processes.end() )
    {
	process->unref();
	_processes.erase( it );
	_updateSetupStateSet = true;
    }

    _lock.writeUnlock();
}

#define MOVE_LAYER( func, cond, inc ) \
void LayeredTexture::func( const LayerProcess* process ) \
{ \
    _lock.writeLock(); \
    std::vector<LayerProcess*>::iterator it = std::find( _processes.begin(), \
	    					    _processes.end(), process);\
    std::vector<LayerProcess*>::iterator neighbor = it inc; \
    if ( it!=_processes.end() && ( cond ) ) \
    { \
	std::swap( it, neighbor ); \
	_updateSetupStateSet = true; \
    } \
 \
    _lock.writeUnlock(); \
}

MOVE_LAYER( moveProcessEarlier, it!=_processes.begin(), -1 )
MOVE_LAYER( moveProcessLater, neighbor!=_processes.end(), +1 )

osg::StateSet* LayeredTexture::getSetupStateSet()
{
    updateSetupStateSet();
    return _setupStateSet;
}


osgGeo::Vec2i LayeredTexture::getEnvelope() const
{
    if ( !_dataLayers.size() )
	return osgGeo::Vec2i( 0, 0 );

    std::vector<LayeredTextureData*>::const_iterator it = _dataLayers.begin();
    osgGeo::Vec2i res((*it)->_image->s(), (*it)->_image->t() );
    for ( it++; it!=_dataLayers.end(); it++ )
    {
	const int s = (*it)->_image->s();
	const int t = (*it)->_image->t();
	if ( s>res._v[0] ) res._v[0] = s;
	if ( t>res._v[1] ) res._v[1] = t;
    }
    // TODO: Take scale and origin into account.

    return res;
}


void LayeredTexture::divideAxis( int size, int bricksize,
				 std::vector<int>& origins,
				 std::vector<int>& sizes )
{
    if ( size<1 ) 
	return;

    if ( bricksize<2 )
	bricksize = 2;

    int cur = 0;

    do
    {
	origins.push_back( cur );

	int cursize = bricksize;
//	while ( cur+cursize/2>=size )
//	    cursize /= 2;
	while ( cur+cursize>size )
	    cursize = (cursize+1) / 2;

	int next = cur+cursize-1;
	sizes.push_back( cursize );
	cur = next;
	
//    } while ( cur<size );
    } while ( cur<size-1 );
}


void LayeredTexture::updateSetupStateSet()
{
    _lock.readLock();

    bool needsupdate = _updateSetupStateSet;
    if ( !_setupStateSet )
    {
	_setupStateSet = new osg::StateSet;
	needsupdate = true;
    }

    if ( !needsupdate )
    {
	for ( int idx=nrDataLayers()-1; idx>=0; idx-- )
	{
	    if ( _dataLayers[idx]->_updateSetupStateSet )
	    {
		needsupdate = true;
		break;
	    }
	}
    }

    if ( needsupdate )
    {
	// TODO: construct shaders and put in stateset

	for ( int idx=nrDataLayers()-1; idx>=0; idx-- )
	    _dataLayers[idx]->_updateSetupStateSet = false;

	_updateSetupStateSet = false;
    }

    _lock.readUnlock();
}


unsigned int LayeredTexture::getTextureSize( unsigned short nr )
{
    if ( nr<=256 )
    {
	if ( nr<=16 )
	{
	    if ( nr<=4 )
		return nr<=2 ? nr : 4;

	    return nr<=8 ? 8 : 16;
	}

	if ( nr<=64 )
	    return nr<=32 ? 32 : 64;

	return nr<=128 ? 128 : 256;
    }

    if ( nr<=4096 )
    {
	if ( nr<=1024 )
	    return nr<=512 ? 512 : 1024;

	return nr<=2048 ? 2048 : 4096;
    }

    if ( nr<=16384 )
	return nr<=8192 ? 8192 : 16384;

    return nr<=32768 ? 32768 : 65536;
}


static void copyImageWithStride( const unsigned char* srcImage, unsigned char* tileImage, int nrRows, int rowSize, int stride, int pixelSize )
{
    int rowLen = rowSize*pixelSize;

    if ( !stride )
    {
	memcpy( tileImage, srcImage, rowLen*nrRows );
	return;
    }

    const int srcInc = (rowSize+stride)*pixelSize;
    for ( int idx=0; idx<nrRows; idx++, srcImage+=srcInc, tileImage+=rowLen )
	memcpy( tileImage, srcImage, rowLen );
}


osg::StateSet* LayeredTexture::createCutoutStateSet(const osgGeo::Vec2i& origin, const osgGeo::Vec2i& size, std::vector<LayeredTexture::TextureCoordData>& tcData ) const
{
    tcData.clear();
    osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;

    for ( int idx=nrDataLayers()-1; idx>=0; idx-- )
    {
	osg::Vec2f tc00, tc01, tc10, tc11;
	LayeredTextureData* layer = _dataLayers[idx];

	const osg::Vec2f localOrigin = layer->getLayerCoord( origin );
	const osg::Vec2f localOpposite = layer->getLayerCoord( origin+size-osgGeo::Vec2i(1,1) );
	const osgGeo::Vec2i tileOrigin( floor(localOrigin.x()), floor(localOrigin.y()) );
	const osgGeo::Vec2i tileOpposite( ceil(localOpposite.x()), ceil(localOpposite.y()) );
	const osgGeo::Vec2i minTileSize( tileOpposite - tileOrigin + osgGeo::Vec2i(1,1) );
	const osgGeo::Vec2i tileSize = osgGeo::Vec2i( getTextureSize(minTileSize.x()), getTextureSize(minTileSize.y()) );

	osg::ref_ptr<const osg::Image> sourceImage = layer->_image;
	osg::ref_ptr<osg::Image> tileImage = new osg::Image();
	tileImage->allocateImage( tileSize.x(), tileSize.y(), sourceImage->r(), sourceImage->getPixelFormat(), sourceImage->getDataType(), sourceImage->getPacking() );

	// TODO: Incorporate texture clamping

	if ( true ) 
	{
	    const int pixelSize = sourceImage->getPixelSizeInBits()/8;
	    int offset = tileOrigin.y()*sourceImage->s()+tileOrigin.x();
	    offset *= pixelSize;
	    const int stride = sourceImage->s()-tileSize.x();
	    copyImageWithStride( sourceImage->data()+offset, tileImage->data(), tileSize.y(), tileSize.x(), stride, pixelSize );
	}
	else
	{
	    //Use existing image with stride
	}

	osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D( tileImage.get() );
	stateset->setTextureAttributeAndModes( layer->_textureUnit, texture.get() );

	const float xLowMargin  = (localOrigin.x()-tileOrigin.x()+0.5) / tileSize.x();
	const float yLowMargin  = (localOrigin.y()-tileOrigin.y()+0.5) / tileSize.y();
	const float xHighMargin = (localOpposite.x()-tileOrigin.x()+0.5) / tileSize.x();
	const float yHighMargin = (localOpposite.y()-tileOrigin.y()+0.5) / tileSize.y();

	tc00 = osg::Vec2f( xLowMargin,  yLowMargin );
	tc01 = osg::Vec2f( xHighMargin, yLowMargin );
	tc10 = osg::Vec2f( xLowMargin,  yHighMargin );
	tc11 = osg::Vec2f( xHighMargin, yHighMargin );

	tcData.push_back( TextureCoordData( layer->_textureUnit, tc00, tc01, tc10, tc11 ) );
    }

    return stateset.release();
}


} //namespace
