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
#include <osgGeo/Vec2i>

#include <string.h>
#include <iostream>


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
			    , _powerOf2Image( 0 )
			    , _borderColor( 0.6f, 0.8f, 0.6f, 1.0f )
			{}

    LayeredTextureData*	clone() const;
    osg::Vec2f		getLayerCoord(const osg::Vec2f& global) const;

    TransparencyType	getTransparencyType();

    const int					_id;
    osg::Vec2f					_origin;
    osg::Vec2f					_scale;
    osg::ref_ptr<const osg::Image>		_image;
    mutable osg::ref_ptr<osg::Image>		_powerOf2Image;
    bool					_updateSetupStateSet;
    int						_textureUnit;
    osg::Vec4d					_borderColor;
};


LayeredTextureData* LayeredTextureData::clone() const
{
    LayeredTextureData* res = new LayeredTextureData( _id );
    res->_origin = _origin;
    res->_scale = _scale; 
    res->_updateSetupStateSet = _updateSetupStateSet; 
    res->_textureUnit = _textureUnit;
    res->_borderColor = _borderColor;
    if ( _image )
	res->_image = (osg::Image*) _image->clone(osg::CopyOp::DEEP_COPY_ALL);

    return res;
}


osg::Vec2f LayeredTextureData::getLayerCoord( const osg::Vec2f& global ) const
{
    osg::Vec2f res = osg::Vec2f(global._v[0], global._v[1] )-_origin;
    res.x() /= _scale.x();
    res.y() /= _scale.y();
    
    return res;
}


ColorSequence::~ColorSequence()
{}


LayeredTexture::LayeredTexture()
    : _freeId( 0 )
    , _updateSetupStateSet( false )
    , _envelopeOrigin( 0.0f, 0.0f )
    , _minLayerScale( 1.0f, 1.0f )
    , _maxTileSize( 0.0f, 0.0f )
    , _maxTextureCopySize( 32*32 )
{}


LayeredTexture::LayeredTexture( const LayeredTexture& lt,
				const osg::CopyOp& co )
    : osg::Object( lt, co )
    , _freeId( lt._freeId )
    , _updateSetupStateSet( false )
    , _setupStateSet( 0 )
    , _envelopeOrigin( lt._envelopeOrigin )
    , _minLayerScale( lt._minLayerScale )
    , _maxTileSize( lt._maxTileSize )
    , _maxTextureCopySize( lt._maxTextureCopySize )
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


#define SET_PROP( funcpostfix, type, variable ) \
void LayeredTexture::setDataLayer##funcpostfix( int id, type localvar ) \
{ \
    const int idx = getDataLayerIndex( id ); \
    if ( idx!=-1 ) \
	_dataLayers[idx]->variable = localvar; \
}

SET_PROP( Origin, const osg::Vec2f&, _origin )
SET_PROP( TextureUnit, int, _textureUnit )
SET_PROP( BorderColor, const osg::Vec4d&, _borderColor )


void LayeredTexture::setDataLayerScale( int id, const osg::Vec2f& scale )
{
    const int idx = getDataLayerIndex( id );
    if ( idx!=-1 && scale.x()>=0.0f && scale.y()>0.0f )
	_dataLayers[idx]->_scale = scale; 
}


void LayeredTexture::setDataLayerImage( int id, const osg::Image* image )
{
    const int idx = getDataLayerIndex( id );
    if ( idx==-1 )
	return;

    const int s = getTextureSize( image->s() );
    const int t = getTextureSize( image->t() );

    if ( (s>image->s() || t>image->t()) && s*t<=_maxTextureCopySize )
    {
	osg::Image* imageCopy = new osg::Image( *image );
	imageCopy->scaleImage( s, t, image->r() );
	_dataLayers[idx]->_image = imageCopy;
    }
    else
	_dataLayers[idx]->_image = image; 
}


#define GET_PROP( funcpostfix, type, variable ) \
type LayeredTexture::getDataLayer##funcpostfix( int id ) const \
{ \
    const int idx = getDataLayerIndex( id ); \
    return _dataLayers[idx]->variable; \
}

GET_PROP( Image, const osg::Image*, _image )
GET_PROP( Origin, const osg::Vec2f&, _origin )
GET_PROP( TextureUnit, int, _textureUnit )
GET_PROP( Scale, const osg::Vec2f&, _scale )
GET_PROP( BorderColor, const osg::Vec4d&, _borderColor )

 
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


osg::Vec2f LayeredTexture::calculateEnvelope()
{
    if ( !_dataLayers.size() )
	return osg::Vec2f( 0.0f, 0.0f );

    std::vector<LayeredTextureData*>::const_iterator it = _dataLayers.begin();
    osg::Vec2f minBound = (*it)->_origin;
    osg::Vec2f maxBound = minBound;
    osg::Vec2f minScale = (*it)->_scale;
    osg::Vec2f minNoPow2Size( 0.0f, 0.0f );

    for ( ; it!=_dataLayers.end(); it++ )
    {
	if ( !(*it)->_image )
	    continue;

	const osg::Vec2f layerSize( (*it)->_image->s() * (*it)->_scale.x(),
				    (*it)->_image->t() * (*it)->_scale.y() );

	const osg::Vec2f bound = layerSize + (*it)->_origin;

	if ( bound.x() > maxBound.x() )
	    maxBound.x() = bound.x();
	if ( bound.y() > maxBound.y() )
	    maxBound.y() = bound.y();
	if ( (*it)->_origin.x() < minBound.x() )
	    minBound.x() = (*it)->_origin.x();
	if ( (*it)->_origin.y() < minBound.y() )
	    minBound.y() = (*it)->_origin.y();
	if ( (*it)->_scale.x() < minScale.x() )
	    minScale.x() = (*it)->_scale.x();
	if ( (*it)->_scale.y() < minScale.y() )
	    minScale.y() = (*it)->_scale.y();

	if ( ( minNoPow2Size.x()<=0.0f || layerSize.x()<minNoPow2Size.x()) &&
	     (*it)->_image->s() != getTextureSize((*it)->_image->s()) )
	    minNoPow2Size.x() = layerSize.x();

	if ( ( minNoPow2Size.y()<=0.0f || layerSize.y()<minNoPow2Size.y()) &&
	     (*it)->_image->t() != getTextureSize((*it)->_image->t()) )
	    minNoPow2Size.y() = layerSize.y();
    }

    _envelopeOrigin = minBound;
    _minLayerScale = minScale;
    _maxTileSize = osg::Vec2f( minNoPow2Size.x() / minScale.x(),
			       minNoPow2Size.y() / minScale.y() );

    return maxBound - minBound;
}


void LayeredTexture::divideAxes( const osg::Vec2f& envelope, int brickSize, std::vector<float>& xTickMarks, std::vector<float>& yTickMarks ) const
{
    const int textureSize = getTextureSize( brickSize );
    osgGeo::Vec2i safeTileSize( textureSize, textureSize );

    while ( safeTileSize.x()>_maxTileSize.x() && _maxTileSize.x()>0.0f )
	safeTileSize.x() /= 2;

    while ( safeTileSize.y()>_maxTileSize.y() && _maxTileSize.y()>0.0f )
	safeTileSize.y() /= 2;

    divideAxis( envelope.x()/_minLayerScale.x(), safeTileSize.x(), xTickMarks );
    divideAxis( envelope.y()/_minLayerScale.y(), safeTileSize.y(), yTickMarks );
}


void LayeredTexture::divideAxis( float totalSize, int brickSize,
				 std::vector<float>& tickMarks )
{
    if ( totalSize < 1.0f ) 
	return;

    const int overlap = 2;
    // One to avoid seam (lower LOD needs more), one because layers
    // may mutually disalign.

    const int minBrickSize = getTextureSize( overlap+1 );
    if ( brickSize < minBrickSize )
	brickSize = minBrickSize;

    float cur = 0.0f;

    while ( true )
    {
	tickMarks.push_back( cur );

	if ( cur >= totalSize-1.0f )
	    return;

	if ( cur+brickSize >= totalSize )
	    cur = totalSize-1.0f;
	else 
	    cur += brickSize-overlap;
    } 
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


static void boundedCopy( unsigned char* dest, const unsigned char* src, int len, const unsigned char* lowPtr, const unsigned char* highPtr )
{
    if ( src>=highPtr || src+len<=lowPtr )
    {
	std::cerr << "Unsafe memcpy" << std::endl;
	return;
    }

    if ( src < lowPtr )
    {
	std::cerr << "Unsafe memcpy" << std::endl;
	len -= lowPtr - src;
	src = lowPtr;
    }
    if ( src+len > highPtr )
    {
	std::cerr << "Unsafe memcpy" << std::endl;
	len = highPtr - src;
    }

    memcpy( dest, src, len );
}


static void copyImageWithStride( const unsigned char* srcImage, unsigned char* tileImage, int nrRows, int rowSize, int offset, int stride, int pixelSize, const unsigned char* srcEnd )
{
    int rowLen = rowSize*pixelSize;
    const unsigned char* srcPtr = srcImage+offset;

    if ( !stride )
    {
	boundedCopy( tileImage, srcPtr, rowLen*nrRows, srcImage, srcEnd);
	return;
    }

    const int srcInc = (rowSize+stride)*pixelSize;
    for ( int idx=0; idx<nrRows; idx++, srcPtr+=srcInc, tileImage+=rowLen )
	boundedCopy( tileImage, srcPtr, rowLen, srcImage, srcEnd );
}


osg::StateSet* LayeredTexture::createCutoutStateSet(const osg::Vec2f& origin, const osg::Vec2f& opposite, std::vector<LayeredTexture::TextureCoordData>& tcData ) const
{
    tcData.clear();
    osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;

    osg::Vec2f globalOrigin( _minLayerScale.x() * origin.x(),
			     _minLayerScale.y() * origin.y() );
    globalOrigin += _envelopeOrigin;

    osg::Vec2f globalOpposite( _minLayerScale.x() * opposite.x(),
			       _minLayerScale.y() * opposite.y() );
    globalOpposite += _envelopeOrigin;

    for ( int idx=nrDataLayers()-1; idx>=0; idx-- )
    {
	osg::Vec2f tc00, tc01, tc10, tc11;
	LayeredTextureData* layer = _dataLayers[idx];

	const osg::Vec2f localOrigin = layer->getLayerCoord( globalOrigin );
	const osg::Vec2f localOpposite = layer->getLayerCoord( globalOpposite );

	const osg::Image* srcImage = layer->_image;
	if ( !srcImage || !srcImage->s() || !srcImage->t() )
	    continue;

	osgGeo::Vec2i size( ceil(localOpposite.x()) + 1,
			    ceil(localOpposite.y()) + 1 );

	osgGeo::Vec2i overshoot( size.x()-srcImage->s(),
				 size.y()-srcImage->t() );
	if ( overshoot.x() > 0 )
	{
	    size.x() -= overshoot.x();
	    overshoot.x() = 0;
	}
	if ( overshoot.y() > 0 )
	{
	    size.y() -= overshoot.y();
	    overshoot.y() = 0;
	}

	osgGeo::Vec2i tileOrigin( floor(localOrigin.x()),
				  floor(localOrigin.y()) );
	if ( tileOrigin.x() < 0 )
	    tileOrigin.x() = 0;
	else
	    size.x() -= tileOrigin.x();

	if ( tileOrigin.y() < 0 )
	    tileOrigin.y() = 0;
	else
	    size.y() -= tileOrigin.y();

	if ( size.x()<1 || size.y()<1 )
	{
	    size = osgGeo::Vec2i( 1, 1 );
	    tileOrigin = osgGeo::Vec2i( 0, 0 );
	}

	const osgGeo::Vec2i tileSize( getTextureSize(size.x()),
				      getTextureSize(size.y()) );
	overshoot += tileSize - size;

	if ( tileOrigin.x()<overshoot.x() || tileOrigin.y()<overshoot.y() )
	{
	    std::cerr << "Unexpected texture size mismatch!" << std::endl;
	    overshoot = osgGeo::Vec2i( 0, 0 );
	    const_cast<osgGeo::Vec2i&>(tileSize) = size;
	}

	if ( overshoot.x() > 0 )
	    tileOrigin.x() -= overshoot.x();
	if ( overshoot.y() > 0 )
	    tileOrigin.y() -= overshoot.y();

	osg::ref_ptr<osg::Image> tileImage = new osg::Image;

	if ( true )
	{
	    tileImage->allocateImage( tileSize.x(), tileSize.y(), srcImage->r(), srcImage->getPixelFormat(), srcImage->getDataType(), srcImage->getPacking() );

	    const int pixelSize = srcImage->getPixelSizeInBits()/8;
	    int offset = tileOrigin.y()*srcImage->s()+tileOrigin.x();
	    offset *= pixelSize;
	    const int stride = srcImage->s()-tileSize.x();
	    const unsigned char* sourceEnd = srcImage->data() + srcImage->s()*srcImage->t()*srcImage->r()*pixelSize;  

	    copyImageWithStride( srcImage->data(), tileImage->data(), tileSize.y(), tileSize.x(), offset, stride, pixelSize, sourceEnd );
	}

	osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D( tileImage );
	texture->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_BORDER );
	texture->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_BORDER );
	texture->setBorderColor( layer->_borderColor );

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


TransparencyType LayeredTexture::LayerProcess::getTransparencyType( const LayeredTexture*) const
{ return Opaque; }


void ColTabLayerProcess::setTextureCoord( float )
{}



void ColTabLayerProcess::setTexturePtr( unsigned char* rgba )
{}


const char* ColTabLayerProcess::getShaderText(void) const
{ return 0; }


} //namespace
