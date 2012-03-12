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

#include <osg/BlendFunc>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/Version>
#include <osgUtil/CullVisitor>
#include <osgGeo/Vec2i>

#include <string.h>
#include <iostream>
#include <cstdio>

#define NR_TEXTURE_UNITS 4

#if OSG_MIN_VERSION_REQUIRED(3,1,0)
     #define USE_IMAGE_STRIDE true
#else
     #define USE_IMAGE_STRIDE false
#endif


namespace osgGeo
{


static TransparencyType getTransparencyTypeBytewise( const unsigned char* start, const unsigned char* stop, int step, const float* borderOpacity=0 )
{
    bool foundOpaquePixel = borderOpacity && *borderOpacity>=1.0f;
    bool foundTransparentPixel = borderOpacity && *borderOpacity<=0.0f;

    for ( const unsigned char* ptr=start; ptr<=stop; ptr+=step )
    {
	if ( *ptr==0 )
	    foundTransparentPixel = true;
	else if ( *ptr==255 )
	    foundOpaquePixel = true;
	else
	    return HasTransparencies;
    }
    if ( foundOpaquePixel && foundTransparentPixel )
	return OnlyFullTransparencies;

    return foundOpaquePixel ? Opaque : FullyTransparent;
}


static TransparencyType getImageTransparencyType( const osg::Image* image, const float* borderOpacity=0, int channel=-1 )
{
    if ( !image )
	return FullyTransparent;

    const bool checkAlpha = channel<0 || channel>2;
    const GLenum format = image->getPixelFormat();
   
    const bool isAlphaFormat = format==GL_RGBA || format==GL_BGRA || format==GL_LUMINANCE_ALPHA || format==GL_ALPHA || format==GL_INTENSITY;

    const bool noAlphaFormat = format==GL_RGB || format==GL_BGR || format==GL_RED || format==GL_GREEN || format==GL_BLUE || format==GL_LUMINANCE || format==GL_DEPTH_COMPONENT;

    if ( noAlphaFormat && checkAlpha )
	return Opaque;

    if ( (format==GL_RED  && channel!=0) || (format==GL_GREEN && channel!=1) ||
	 (format==GL_BLUE && channel!=2) || (format==GL_ALPHA && !checkAlpha) )
	return FullyTransparent;

    const GLenum dataType = image->getDataType();
    const bool isByte = dataType==GL_UNSIGNED_BYTE || dataType==GL_BYTE;
    if ( isByte && (isAlphaFormat || noAlphaFormat) )
    {
	const int step = image->getPixelSizeInBits()/8;
	const int offset = step==2 && checkAlpha ? 1 : (step<3 ? 0 : channel);
	const unsigned char* start = image->data()+offset;
	const unsigned char* stop = start+image->getTotalSizeInBytes()-step;
	return getTransparencyTypeBytewise( start, stop, step, borderOpacity ); 
    }

    const int idx = channel>=0 && channel<4 ? channel : 3; 
    bool foundOpaquePixel = borderOpacity && *borderOpacity>=1.0f;
    bool foundTransparentPixel = borderOpacity && *borderOpacity<=0.0f;

    for ( int r=0; r<image->r(); r++ )
    {
	for ( int t=0; t<image->t(); t++ )
	{
	    for ( int s=0; s<image->s(); s++ )
	    {
		const float val = image->getColor(s,t,r)[idx];
		if ( val<=0.0f )
		    foundTransparentPixel = true;
		else if ( val>=1.0f )
		    foundOpaquePixel = true;
		else
		    return HasTransparencies;
	    }
	}
    }

    if ( foundOpaquePixel && foundTransparentPixel )
	return OnlyFullTransparencies;

    return foundOpaquePixel ? Opaque : FullyTransparent;
}


ColorSequence::ColorSequence( unsigned char* array )
    : _arr( array )
    , _dirtyCount( 0 )
    , _transparencyType( TransparencyUnknown )
{
    if ( array )
	setRGBAValues( array );
}


void ColorSequence::setRGBAValues( unsigned char* array )
{
    _arr = array;
    touch();
}


void ColorSequence::touch()
{
    _dirtyCount++;
    _transparencyType = TransparencyUnknown;
}


TransparencyType ColorSequence::getTransparencyType() const
{
    if ( _transparencyType==TransparencyUnknown )
    {
	if ( !_arr )
	    _transparencyType = FullyTransparent;
	else 
	    _transparencyType = getTransparencyTypeBytewise( _arr+3, _arr+1023, 4 );
    }

    return _transparencyType;
}


struct LayeredTextureData : public osg::Referenced
{
			LayeredTextureData(int id)
			    : _id( id )
			    , _origin( 0.0f, 0.0f )
			    , _scale( 1.0f, 1.0f )
			    , _updateSetupStateSet( true )
			    , _textureUnit( -1 )
			    , _image( 0 )
			    , _imageScale( 1.0f, 1.0f )
			    , _imageTransparencyType(TransparencyUnknown)
			    , _borderColor( 0.6f, 0.8f, 0.6f, 1.0f )
			    , _isOn( true )
			{}

    LayeredTextureData*	clone() const;
    osg::Vec2f		getLayerCoord(const osg::Vec2f& global) const;

    TransparencyType	getTransparencyType();

    const int					_id;
    osg::Vec2f					_origin;
    osg::Vec2f					_scale;
    osg::ref_ptr<const osg::Image>		_image;
    osg::Vec2f					_imageScale;
    TransparencyType				_imageTransparencyType;
    bool					_updateSetupStateSet;
    int						_textureUnit;
    osg::Vec4d					_borderColor;
    bool					_isOn;
};


LayeredTextureData* LayeredTextureData::clone() const
{
    LayeredTextureData* res = new LayeredTextureData( _id );
    res->_origin = _origin;
    res->_scale = _scale; 
    res->_updateSetupStateSet = _updateSetupStateSet; 
    res->_textureUnit = _textureUnit;
    res->_borderColor = _borderColor;
    res->_isOn = _isOn;
    res->_imageScale = _imageScale; 
    res->_imageTransparencyType = _imageTransparencyType; 
    if ( _image )
	res->_image = (osg::Image*) _image->clone(osg::CopyOp::DEEP_COPY_ALL);
   
    return res;
}


osg::Vec2f LayeredTextureData::getLayerCoord( const osg::Vec2f& global ) const
{
    osg::Vec2f res = global - _origin;
    res.x() /= _scale.x() * _imageScale.x();
    res.y() /= _scale.y() * _imageScale.y();
    
    return res;
}


TransparencyType LayeredTextureData::getTransparencyType()
{
    if ( !_image || !_isOn )
	return FullyTransparent;

    if ( _imageTransparencyType==TransparencyUnknown )
    {
	const float borderOpacity = _borderColor.a();
	_imageTransparencyType = getImageTransparencyType( _image, &borderOpacity );
    }

    return _imageTransparencyType;
}


struct TilingInfo
{
			TilingInfo()			{ reInit(); }

    void		reInit()
			{
			    _envelopeOrigin = osg::Vec2f( 0.0f, 0.0f );
			    _envelopeSize = osg::Vec2f( 0.0f, 0.0f );
			    _smallestScale = osg::Vec2f( 1.0f, 1.0f );
			    _maxTileSize = osg::Vec2f( 0.0f, 0.0f );
			    _needsUpdate = false;
			    _retilingNeeded = true;
			}

    osg::Vec2f		_envelopeOrigin;
    osg::Vec2f		_envelopeSize;
    osg::Vec2f		_smallestScale;
    osg::Vec2f  	_maxTileSize;
    bool		_needsUpdate;
    bool		_retilingNeeded;
};


ColorSequence::~ColorSequence()
{}


LayeredTexture::LayeredTexture()
    : _freeId( 1 )
    , _updateSetupStateSet( false )
    , _maxTextureCopySize( 32*32 )
    , _tilingInfo( new TilingInfo )
{}


LayeredTexture::LayeredTexture( const LayeredTexture& lt,
				const osg::CopyOp& co )
    : osg::Object( lt, co )
    , _freeId( lt._freeId )
    , _updateSetupStateSet( false )
    , _setupStateSet( 0 )
    , _maxTextureCopySize( lt._maxTextureCopySize )
    , _tilingInfo( new TilingInfo(*lt._tilingInfo) )
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

    delete _tilingInfo;
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
	_tilingInfo->_needsUpdate = true;
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

SET_PROP( TextureUnit, int, _textureUnit )
SET_PROP( BorderColor, const osg::Vec4d&, _borderColor )


void LayeredTexture::setDataLayerOrigin( int id, const osg::Vec2f& origin )
{
    const int idx = getDataLayerIndex( id );
    if ( idx!=-1 )
    {
	_dataLayers[idx]->_origin = origin; 
	_tilingInfo->_needsUpdate = true;
    }
}


void LayeredTexture::setDataLayerScale( int id, const osg::Vec2f& scale )
{
    const int idx = getDataLayerIndex( id );
    if ( idx!=-1 && scale.x()>=0.0f && scale.y()>0.0f )
    {
	_dataLayers[idx]->_scale = scale; 
	_tilingInfo->_needsUpdate = true;
    }
}


void LayeredTexture::setDataLayerImage( int id, const osg::Image* image, TransparencyType transparencyType )
{
    const int idx = getDataLayerIndex( id );
    if ( idx==-1 )
	return;

    if ( transparencyType!=TransparencyUnchanged )
	_dataLayers[idx]->_imageTransparencyType = transparencyType;

    if ( image )
    {
	const int s = getTextureSize( image->s() );
	const int t = getTextureSize( image->t() );

	if ( (s>image->s() || t>image->t()) && s*t<=_maxTextureCopySize )
	{
	    osg::Image* imageCopy = new osg::Image( *image );
	    imageCopy->scaleImage( s, t, image->r() );
	    _dataLayers[idx]->_image = imageCopy;
	    _dataLayers[idx]->_imageScale.x() = float(image->s()) / float(s);
	    _dataLayers[idx]->_imageScale.y() = float(image->t()) / float(t);
	    _tilingInfo->_needsUpdate = true;
	    return;
	}
	else if ( USE_IMAGE_STRIDE && _dataLayers[idx]->_image.get()==image )
	    return;
    }

    _dataLayers[idx]->_image = image; 
    _dataLayers[idx]->_imageScale = osg::Vec2f( 1.0f, 1.0f );
    _tilingInfo->_needsUpdate = true;
}


#define GET_PROP( funcpostfix, type, variable, undefval ) \
type LayeredTexture::getDataLayer##funcpostfix( int id ) const \
{ \
    const int idx = getDataLayerIndex( id ); \
    static type undefvar = undefval; \
    return idx==-1 ? undefvar : _dataLayers[idx]->variable; \
}

GET_PROP( Image, const osg::Image*, _image.get(), 0 )
GET_PROP( Origin, const osg::Vec2f&, _origin, osg::Vec2f(0.0f,0.0f) )
GET_PROP( TextureUnit, int, _textureUnit, -1 )
GET_PROP( Scale, const osg::Vec2f&, _scale, osg::Vec2f(1.0f,1.0f) )
GET_PROP( BorderColor, const osg::Vec4d&, _borderColor, osg::Vec4d(0.0f,0.0f,0.0f,0.0f) )
GET_PROP( TransparencyType, TransparencyType, getTransparencyType(), FullyTransparent )


void LayeredTexture::turnDataLayerOn( int id, bool yn )
{
    const int idx = getDataLayerIndex( id );
    if ( idx!=-1 )
    {
	_dataLayers[idx]->_isOn = true; 
    }
}


bool LayeredTexture::isDataLayerOn( int id ) const
{
    const int idx = getDataLayerIndex( id );
    return idx!=-1 && _dataLayers[idx]->_isOn;
}


LayerProcess* LayeredTexture::getProcess( int idx )
{ return idx>=0 && idx<_processes.size() ? _processes[idx] : 0;  }


const LayerProcess* LayeredTexture::getProcess( int idx ) const
{ return const_cast<LayeredTexture*>(this)->getProcess(idx); }


void LayeredTexture::addProcess( LayerProcess* process )
{
    if ( !process )
	return;

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
    if ( it!=_processes.end() ) \
    { \
	std::vector<LayerProcess*>::iterator neighbor = it inc; \
	if ( cond ) \
	{ \
	    std::swap( *it, *neighbor ); \
	    _updateSetupStateSet = true; \
	} \
    } \
    _lock.writeUnlock(); \
}

MOVE_LAYER( moveProcessEarlier, it!=_processes.begin(), -1 )
MOVE_LAYER( moveProcessLater, neighbor!=_processes.end(), +1 )

osg::StateSet* LayeredTexture::getSetupStateSet()
{
    updateSetupStateSet();
    return _setupStateSet;
}


void LayeredTexture::updateTilingInfoIfNeeded() const
{
    if ( !_tilingInfo->_needsUpdate )
	return;

    _tilingInfo->reInit();

    if ( !_dataLayers.size() )
	return;

    std::vector<LayeredTextureData*>::const_iterator it = _dataLayers.begin();
    osg::Vec2f minBound = (*it)->_origin;
    osg::Vec2f maxBound = minBound;
    osg::Vec2f minScale( 0.0f, 0.0f );
    osg::Vec2f minNoPow2Size( 0.0f, 0.0f );

    for ( ; it!=_dataLayers.end(); it++ )
    {
	if ( !(*it)->_image )
	    continue;

	const osg::Vec2f scale( (*it)->_scale.x() * (*it)->_imageScale.x(),
				(*it)->_scale.y() * (*it)->_imageScale.y() );

	const osg::Vec2f layerSize( (*it)->_image->s() * scale.x(),
				    (*it)->_image->t() * scale.y() );

	const osg::Vec2f bound = layerSize + (*it)->_origin;

	if ( bound.x() > maxBound.x() )
	    maxBound.x() = bound.x();
	if ( bound.y() > maxBound.y() )
	    maxBound.y() = bound.y();
	if ( (*it)->_origin.x() < minBound.x() )
	    minBound.x() = (*it)->_origin.x();
	if ( (*it)->_origin.y() < minBound.y() )
	    minBound.y() = (*it)->_origin.y();
	if ( minScale.x()<=0.0f || scale.x()<minScale.x() )
	    minScale.x() = scale.x();
	if ( minScale.y()<=0.0f || scale.y()<minScale.y() )
	    minScale.y() = scale.y();

	if ( ( minNoPow2Size.x()<=0.0f || layerSize.x()<minNoPow2Size.x()) &&
	     (*it)->_image->s() != getTextureSize((*it)->_image->s()) )
	    minNoPow2Size.x() = layerSize.x();

	if ( ( minNoPow2Size.y()<=0.0f || layerSize.y()<minNoPow2Size.y()) &&
	     (*it)->_image->t() != getTextureSize((*it)->_image->t()) )
	    minNoPow2Size.y() = layerSize.y();
    }

    _tilingInfo->_envelopeSize = maxBound - minBound;
    _tilingInfo->_envelopeOrigin = minBound;
    _tilingInfo->_smallestScale = minScale;
    _tilingInfo->_maxTileSize = osg::Vec2f( minNoPow2Size.x() / minScale.x(),
					    minNoPow2Size.y() / minScale.y() );
}


bool LayeredTexture::needsRetiling() const
{
    updateTilingInfoIfNeeded();
    return _tilingInfo->_retilingNeeded;
}


osg::Vec2f LayeredTexture::envelopeSize() const
{
    updateTilingInfoIfNeeded();
    return _tilingInfo->_envelopeSize;
}


osg::Vec2f LayeredTexture::envelopeCenter() const
{
    updateTilingInfoIfNeeded();
    return _tilingInfo->_envelopeOrigin + _tilingInfo->_envelopeSize*0.5;
}


void LayeredTexture::planTiling( int brickSize, std::vector<float>& xTickMarks, std::vector<float>& yTickMarks ) const
{
    updateTilingInfoIfNeeded();
    _tilingInfo->_retilingNeeded = false; 

    const int textureSize = getTextureSize( brickSize );
    osgGeo::Vec2i safeTileSize( textureSize, textureSize );

    const osg::Vec2f& maxTileSize = _tilingInfo->_maxTileSize;
    while ( safeTileSize.x()>maxTileSize.x() && maxTileSize.x()>0.0f )
	safeTileSize.x() /= 2;

    while ( safeTileSize.y()>maxTileSize.y() && maxTileSize.y()>0.0f )
	safeTileSize.y() /= 2;

    const osg::Vec2f& size = _tilingInfo->_envelopeSize;
    const osg::Vec2f& minScale = _tilingInfo->_smallestScale;
    divideAxis( size.x()/minScale.x(), safeTileSize.x(), xTickMarks );
    divideAxis( size.y()/minScale.y(), safeTileSize.y(), yTickMarks );
}


void LayeredTexture::divideAxis( float totalSize, int brickSize,
				 std::vector<float>& tickMarks )
{
    if ( totalSize <= 1.0f ) 
    {
	tickMarks.push_back( 0.0f );
	tickMarks.push_back( 1.0f );
	return;
    }

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


static void copyImageTile( const osg::Image& srcImage, osg::Image& tileImage, const osgGeo::Vec2i& tileOrigin, const osgGeo::Vec2i& tileSize )
{
    tileImage.allocateImage( tileSize.x(), tileSize.y(), srcImage.r(), srcImage.getPixelFormat(), srcImage.getDataType(), srcImage.getPacking() );

    const int pixelSize = srcImage.getPixelSizeInBits()/8;
    int offset = tileOrigin.y()*srcImage.s()+tileOrigin.x();
    offset *= pixelSize;
    const int stride = srcImage.s()-tileSize.x();
    const unsigned char* sourceEnd = srcImage.data() + srcImage.s()*srcImage.t()*srcImage.r()*pixelSize;  

    copyImageWithStride( srcImage.data(), tileImage.data(), tileSize.y(), tileSize.x(), offset, stride, pixelSize, sourceEnd );
}


osg::StateSet* LayeredTexture::createCutoutStateSet(const osg::Vec2f& origin, const osg::Vec2f& opposite, std::vector<LayeredTexture::TextureCoordData>& tcData ) const
{
    tcData.clear();
    osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;

    osg::Vec2f globalOrigin( _tilingInfo->_smallestScale.x() * origin.x(),
			     _tilingInfo->_smallestScale.y() * origin.y() );
    globalOrigin += _tilingInfo->_envelopeOrigin;

    osg::Vec2f globalOpposite( _tilingInfo->_smallestScale.x() * opposite.x(),
			       _tilingInfo->_smallestScale.y() * opposite.y() );
    globalOpposite += _tilingInfo->_envelopeOrigin;

    for ( int idx=nrDataLayers()-1; idx>=0; idx-- )
    {
	LayeredTextureData* layer = _dataLayers[idx];
	if ( layer->_textureUnit < 0 )
	    continue;

	const osg::Vec2f localOrigin = layer->getLayerCoord( globalOrigin );
	const osg::Vec2f localOpposite = layer->getLayerCoord( globalOpposite );

	const osg::Image* srcImage = layer->_image;
	if ( !srcImage || !srcImage->s() || !srcImage->t() )
	    continue;

	osgGeo::Vec2i size( ceil(localOpposite.x()+0.5),
			    ceil(localOpposite.y()+0.5) );

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

	osgGeo::Vec2i tileOrigin( floor(localOrigin.x()-0.5),
				  floor(localOrigin.y()-0.5) );
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

#if USE_IMAGE_STRIDE
	osg::ref_ptr<osg::Image> si = const_cast<osg::Image*>(srcImage);
	tileImage->setUserData( si.get() );
	tileImage->setImage( tileSize.x(), tileSize.y(), si->r(), si->getInternalTextureFormat(), si->getPixelFormat(), si->getDataType(), si->data(tileOrigin.x(),tileOrigin.y()), osg::Image::NO_DELETE, si->getPacking(), si->s() ); 
#else
	copyImageTile( *srcImage, *tileImage, tileOrigin, tileSize );
#endif

	osg::Vec2f tc00, tc01, tc10, tc11;
	tc00.x() = (localOrigin.x()-tileOrigin.x()+0.5) / tileSize.x();
	tc00.y() = (localOrigin.y()-tileOrigin.y()+0.5) / tileSize.y();
	tc11.x() = (localOpposite.x()-tileOrigin.x()+0.5) / tileSize.x();
	tc11.y() = (localOpposite.y()-tileOrigin.y()+0.5) / tileSize.y();
	tc01 = osg::Vec2f( tc11.x(), tc00.y() );
	tc10 = osg::Vec2f( tc00.x(), tc11.y() );

	tcData.push_back( TextureCoordData( layer->_textureUnit, tc00, tc01, tc10, tc11 ) );

	osg::Texture::WrapMode xWrapMode = osg::Texture::CLAMP_TO_EDGE;
	if ( tc00.x()<0.0f || tc11.x()>1.0f )
	    xWrapMode = osg::Texture::CLAMP_TO_BORDER;

	osg::Texture::WrapMode yWrapMode = osg::Texture::CLAMP_TO_EDGE;
	if ( tc00.y()<0.0f || tc11.y()>1.0f )
	    yWrapMode = osg::Texture::CLAMP_TO_BORDER;

	osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D( tileImage );
	texture->setWrap( osg::Texture::WRAP_S, xWrapMode );
	texture->setWrap( osg::Texture::WRAP_T, yWrapMode );
	texture->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
	texture->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
	texture->setBorderColor( layer->_borderColor );

	stateset->setTextureAttributeAndModes( layer->_textureUnit, texture.get() );

	char texelSizeName[20];
	sprintf( texelSizeName, "texelsize%d", layer->_textureUnit );
	const osg::Vec2f texelSize( 1.0/tileSize.x(), 1.0/tileSize.y() );
	stateset->addUniform( new osg::Uniform(texelSizeName, texelSize) );
    }

    return stateset.release();
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
	std::vector<LayeredTextureData*>::iterator it = _dataLayers.begin();
	for ( ; it!=_dataLayers.end(); it++ )
	{
	    if ( (*it)->_updateSetupStateSet )
	    {
		needsupdate = true;
		break;
	    }
	}
    }

    if ( needsupdate )
    {
	buildShaders();

	std::vector<LayeredTextureData*>::iterator it = _dataLayers.begin();
	for ( ; it!=_dataLayers.end(); it++ )
	    (*it)->_updateSetupStateSet = false;

	_updateSetupStateSet = false;
    }

    _lock.readUnlock();
}


void LayeredTexture::buildShaders()
{
    int nrUsedLayers;
    std::vector<int> orderedLayerIDs;
    bool resultIsOpaque;
    const int nrProc = getProcessInfo( orderedLayerIDs, nrUsedLayers, &resultIsOpaque );

    bool needColSeqTexture = false;
    int minUnit = NR_TEXTURE_UNITS;
    std::vector<int> activeUnits;

    std::vector<int>::iterator it = orderedLayerIDs.begin();
    for ( ; nrUsedLayers>0; it++, nrUsedLayers-- )
    {
	if ( *it )
	{
	    const int unit = getDataLayerTextureUnit( *it );
	    activeUnits.push_back( unit );
	    if ( unit<minUnit )
		minUnit = unit;
	}
	else
	    needColSeqTexture = true;
    }

    if ( minUnit<0 || (minUnit==0 && needColSeqTexture) )
    {
	_tilingInfo->_retilingNeeded = true;
	return;
    }

    _setupStateSet->clear();

    std::string code;
    getVertexShaderCode( code, activeUnits );
    osg::ref_ptr<osg::Shader> vertexShader = new osg::Shader( osg::Shader::VERTEX, code );

    if ( needColSeqTexture )
    {
	createColSeqTexture();
	activeUnits.push_back( 0 );
    }

    getFragmentShaderCode( code, activeUnits, nrProc );
    osg::ref_ptr<osg::Shader> fragmentShader = new osg::Shader( osg::Shader::FRAGMENT, code );

    osg::ref_ptr<osg::Program> program = new osg::Program;
    //program->ref();

    program->addShader( vertexShader.get() );
    program->addShader( fragmentShader.get() );
    _setupStateSet->setAttributeAndModes( program.get() );

    char samplerName[20];
    for ( it=activeUnits.begin(); it!=activeUnits.end(); it++ )
    {
	sprintf( samplerName, "texture%d", *it );
	_setupStateSet->addUniform( new osg::Uniform(samplerName, *it) );
    }

    if ( !resultIsOpaque )
    {
	osg::ref_ptr<osg::BlendFunc> blendFunc = new osg::BlendFunc;
	blendFunc->setFunction( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	_setupStateSet->setAttributeAndModes( blendFunc );
	_setupStateSet->setRenderingHint( osg::StateSet::TRANSPARENT_BIN );
    }
    else
	_setupStateSet->setRenderingHint( osg::StateSet::OPAQUE_BIN );
}


int LayeredTexture::getProcessInfo( std::vector<int>& layerIDs, int& nrUsedLayers, bool* resultIsOpaque ) const
{
    layerIDs.empty();
    std::vector<int> skippedIDs;
    int nrProc = 0;
    nrUsedLayers = 0;

    if ( resultIsOpaque  )
	*resultIsOpaque = false;

    std::vector<LayerProcess*>::const_reverse_iterator it = _processes.rbegin();
    for ( ; it!=_processes.rend(); it++ )
    {
	const TransparencyType transparency = (*it)->getTransparencyType(this);
	int nrPushed = 0;
	for ( int idx=-1; ; idx++ )
	{
	    int id = 0;		// ColSeqTexture represented by ID=0

	    if ( idx>=0 )
		id = (*it)->getDataLayerID( idx );
	    else if ( !(*it)->needsColorSequence() )
		continue;

	    if ( id<0 )
	    {
		if ( idx<=3 )
		    continue;
		else
		    break;
	    }

	    const std::vector<int>::iterator it1 = std::find(layerIDs.begin(),layerIDs.end(),id);
	    const std::vector<int>::iterator it2 = std::find(skippedIDs.begin(),skippedIDs.end(),id);

	    if ( !nrUsedLayers )
	    {

		if ( transparency!=FullyTransparent )
		{
		    if ( it2 != skippedIDs.end() )
			skippedIDs.erase( it2 );
		}
		else if ( it1==layerIDs.end() && it2==skippedIDs.end() )
		    skippedIDs.push_back( id );
	    }


	    if ( it1==layerIDs.end() )
	    {
		if ( !nrUsedLayers || transparency!=FullyTransparent )
		{
		    layerIDs.push_back( id );
		    nrPushed++;
		}
	    }
	}

	if ( !nrUsedLayers )
	{
	    const int sz = layerIDs.size();
	    if ( sz > NR_TEXTURE_UNITS )
		nrUsedLayers = sz-nrPushed; // Drop early layers if no room
	    else
	    {
		nrProc++;
		if ( transparency==Opaque )
		{
		    nrUsedLayers = sz;
		    if ( resultIsOpaque )
			*resultIsOpaque = true;
		}
	    }
	}
    }

    if ( !nrUsedLayers )
	nrUsedLayers = layerIDs.size();

    layerIDs.insert( layerIDs.begin()+nrUsedLayers,
		     skippedIDs.begin(), skippedIDs.end() );
    return nrProc;
}


void LayeredTexture::createColSeqTexture()
{
    osg::ref_ptr<osg::Image> colSeqImage = new osg::Image();
    const int nrProc = nrProcesses();
    const int texSize = getTextureSize( nrProc );
    colSeqImage->allocateImage( 256, texSize, 1, GL_RGBA, GL_UNSIGNED_BYTE );

    const int rowSize = colSeqImage->getRowSizeInBytes();
    std::vector<LayerProcess*>::const_iterator it = _processes.begin();
    for ( int idx=0; idx<nrProc; idx++, it++ )
    {
	const unsigned char* ptr = (*it)->getColorSequencePtr();
	if ( ptr )
	    memcpy( colSeqImage->data(0,idx), ptr, rowSize );

	(*it)->setColorSequenceTextureCoord( (idx+0.5)/texSize );
    }
    osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D( colSeqImage );
    texture->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
    texture->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
    _setupStateSet->setTextureAttributeAndModes( 0, texture.get() ); 
}


void LayeredTexture::assignTextureUnits()
{
    std::vector<LayeredTextureData*>::iterator lit = _dataLayers.begin();
    for ( ; lit!=_dataLayers.end(); lit++ )
	(*lit)->_textureUnit = -1;
    
    std::vector<int> orderedLayerIDs;
    int nrUsedLayers;
    getProcessInfo( orderedLayerIDs, nrUsedLayers );

    int unit = 0;	// Reserved for ColSeqTexture if needed

    const bool preloadUnusedLayers = false;
    if ( preloadUnusedLayers )
	nrUsedLayers = NR_TEXTURE_UNITS;

    std::vector<int>::iterator iit = orderedLayerIDs.begin();
    for ( ; iit!=orderedLayerIDs.end() && nrUsedLayers>0; iit++ )
    {
	if ( (*iit)>0 )
	    setDataLayerTextureUnit( *iit, (++unit)%NR_TEXTURE_UNITS );

	nrUsedLayers--;
    }
    _updateSetupStateSet = true;
}


const char* LayeredTexture::getVertexShaderCode( std::string& code, const std::vector<int>& activeUnits ) const
{
    code =

"void main(void)\n"
"{\n"
"    vec3 fragNormal = normalize(gl_NormalMatrix * gl_Normal);\n"
"\n"
"    vec4 diffuse = vec4(0.0,0.0,0.0,0.0);\n"
"    vec4 ambient = vec4(0.0,0.0,0.0,0.0);\n"
"    vec4 specular = vec4(0.0,0.0,0.0,0.0);\n"
"\n"
"    for ( int light=0; light<2; light++ )\n"
"    {\n"
"        vec3 lightDir = normalize( vec3(gl_LightSource[light].position) );\n"
"        float NdotL = abs( dot(fragNormal, lightDir) );\n"
"\n"
"        diffuse += gl_LightSource[light].diffuse * NdotL;\n"
"        ambient += gl_LightSource[light].ambient;\n"
"        float pf = 0.0;\n"
"        if (NdotL != 0.0)\n"
"        {\n"
"            float nDotHV = abs( \n"
"	          dot(fragNormal, vec3(gl_LightSource[light].halfVector)) );\n"
"            pf = pow( nDotHV, gl_FrontMaterial.shininess );\n"
"        }\n"
"        specular += gl_LightSource[light].specular * pf;\n"
"    }\n"
"\n"
"    gl_FrontColor =\n"
"        gl_FrontLightModelProduct.sceneColor +\n"
"        ambient  * gl_FrontMaterial.ambient +\n"
"        diffuse  * gl_FrontMaterial.diffuse +\n"
"        specular * gl_FrontMaterial.specular;\n"
"\n"
"    gl_Position = ftransform();\n"
"\n";

    char line[80];
    std::vector<int>::const_iterator it = activeUnits.begin();
    for ( ; it!=activeUnits.end(); it++ )
    {
	sprintf( line, "    gl_TexCoord[%d] = gl_TextureMatrix[%d] * gl_MultiTexCoord%d;\n", *it, *it, *it );
	code += line;
    }

    code += "}\n";

    std::cout << code << std::endl;
}


void LayeredTexture::getFragmentShaderCode( std::string& code, const std::vector<int>& activeUnits, int nrProc ) const
{
    code.clear();
    char line[80];
    std::vector<int>::const_iterator it = activeUnits.begin();
    for ( ; it!=activeUnits.end(); it++ )
    {
	sprintf( line, "uniform sampler2D texture%d;\n", *it );
	code += line;
	sprintf( line, "uniform vec2 texelsize%d;\n", *it );
	code += line;
    }

    code += "\n"
	    "void main()\n"
	    "{\n"
	    "    if ( gl_FrontMaterial.diffuse.a<=0.0 )\n"
	    "        discard;\n"
	    "\n"
	    "    vec4 c0, c1, c2, c3;\n"
	    "    vec2 tcoord;\n"
	    "    float f0, f1, a, b;\n\n";

    int stage = 0;
    const int startProc = nrProcesses()-nrProc;

    for ( int idx=0; idx<nrProc; idx++ )
    {
	const LayerProcess* process = _processes[startProc+idx];
	if ( process->getTransparencyType(this)!=FullyTransparent )
	    process->getShaderCode( code, this, stage++ );
    }

    if ( !stage )
	code += "    gl_FragColor = vec4(1.0,1.0,1.0,1.0);\n";

    code += "    gl_FragColor.a *= gl_FrontMaterial.diffuse.a;\n"
	    "    gl_FragColor.rgb *= gl_Color.rgb;\n"
	    "}\n";

    std::cout << code << std::endl;
}


LayerProcess::LayerProcess()
    : _colSeqPtr( 0 )
    , _bilinearFiltering( true )
    , _opacity( 1.0 )
{}


const unsigned char* LayerProcess::getColorSequencePtr() const
{ return _colSeqPtr; }


void LayerProcess::setColorSequenceTextureCoord( float coord )
{ _colSeqTexCoord = coord; }


void LayerProcess::setBilinearFiltering( bool yn )
{ _bilinearFiltering = yn; }


void LayerProcess::setOpacity( float opacity )
{
    if ( opacity>=0.0 && opacity<=1.0 )
       _opacity = opacity;
}


float LayerProcess::getOpacity() const
{ return _opacity; }


void LayerProcess::getHeaderCode( std::string& code, int unit, int toIdx, int fromIdx ) const
{
    char line[80];
    sprintf( line, "    tcoord = gl_TexCoord[%d].st;\n", unit );
    code += line;

    char to[5] = ""; 
    char from[5] = ""; 
    if ( toIdx>=0 )
    {
	 sprintf( to, "[%d]", toIdx );
	 sprintf( from, "[%d]", fromIdx );
    }

    sprintf( line, "    c0%s = texture2D( texture%d, tcoord )%s;\n", to, unit, from );
    code += line;

    if ( !_bilinearFiltering )
	return;

    sprintf( line, "    tcoord[0] += texelsize%d[0];\n", unit );
    code += line;
    sprintf( line, "    c1%s = texture2D( texture%d, tcoord )%s;\n", to, unit, from );
    code += line;
    sprintf( line, "    tcoord[1] += texelsize%d[1];\n", unit );
    code += line;
    sprintf( line, "    c3%s = texture2D( texture%d, tcoord )%s;\n", to, unit, from );
    code += line;
    sprintf( line, "    tcoord[0] -= texelsize%d[0];\n", unit );
    code += line;
    sprintf( line, "    c2%s = texture2D( texture%d, tcoord )%s;\n", to, unit, from );
    code += line;

    sprintf( line, "    f0 = fract( tcoord[0]/texelsize%d[0] );\n", unit );
    code += line;
    sprintf( line, "    f1 = fract( tcoord[1]/texelsize%d[1] );\n", unit );
    code += line;
}


void LayerProcess::getFooterCode( std::string& code, int stage ) const
{
    char line[80];
    sprintf( line, "    a = c0.a * %.9f;\n", getOpacity() );
    code += line;

    if ( !stage )
    {
	code += "    gl_FragColor.rgb = c0.rgb;\n"
		"    gl_FragColor.a = a;\n\n";
    }
    else
    {
	code += "    b = gl_FragColor.a * (1.0-a);\n"
		"    gl_FragColor.a = a + b;\n"
		"    if ( gl_FragColor.a>0.0 )\n"
		"        gl_FragColor.rgb =(a*c0.rgb+b*gl_FragColor.rgb) / gl_FragColor.a;\n\n";
    }
}


ColTabLayerProcess::ColTabLayerProcess()
    : _id( -1 )
    , _textureChannel( 0 )
    , _colorSequence( 0 )
    , _hasUndef( false )
{}


void ColTabLayerProcess::setDataLayerID( int id, int channel )
{
    _id = id;
    _textureChannel = channel>=0 && channel<4 ? channel : 0;
}


int ColTabLayerProcess::getDataLayerID( int idx ) const
{ return idx ? -1 : _id; } 


int ColTabLayerProcess::getDataLayerTextureChannel() const
{ return _textureChannel; }


void ColTabLayerProcess::setDataLayerColorSequence( const ColorSequence* colSeq )
{
    _colorSequence = colSeq;
    _colSeqPtr = colSeq->getRGBAValues();
}


const ColorSequence* ColTabLayerProcess::getDataLayerColorSequence() const
{ return _colorSequence; }


void ColTabLayerProcess::setDataLayerHasUndef( bool hasUdf )
{ _hasUndef = hasUdf; }


bool ColTabLayerProcess::getDataLayerHasUndef() const
{ return _hasUndef; }


void ColTabLayerProcess::getShaderCode( std::string& code, const LayeredTexture* layTex, int stage ) const
{
    if ( !layTex || layTex->getDataLayerIndex(_id)<0 )
	return;

    getHeaderCode( code, layTex->getDataLayerTextureUnit(_id) );
    
    char line[80];
    sprintf( line, "\n    tcoord[1] = %.9f;\n", _colSeqTexCoord );
    code += line;

    for ( int idx=0; idx<4; idx++ )
    {				     // (255.0/256)*val+(0.5/256)
	sprintf( line, "    tcoord[0] = 0.996093750*c%d[%d] + 0.001953125;\n", idx, _textureChannel );
	code += line;
	sprintf( line, "    c%d = texture2D( texture0, tcoord );\n", idx );
	code += line;

	if ( !_bilinearFiltering )
	    break;
    }

    if ( _bilinearFiltering )
	code += "    c0 = mix( mix(c0,c1,f0), mix(c2,c3,f0), f1 );\n";

    code += "\n";
    getFooterCode( code, stage );
}


TransparencyType ColTabLayerProcess::getTransparencyType( const LayeredTexture* layTex ) const
{
    const ColorSequence* colSeq = getDataLayerColorSequence();
    if ( !colSeq || _opacity==0.0 || getDataLayerID()<0 )
	return FullyTransparent;

    const TransparencyType tt = colSeq->getTransparencyType();
    // Only optimal if all colors in sequence are actually used

    if ( _opacity==1.0 )
	return tt;

    return tt==FullyTransparent ? tt : HasTransparencies;
}


bool ColTabLayerProcess::doProcess( const LayeredTexture*, osg::Image* output )
{ return false; }


RGBALayerProcess::RGBALayerProcess()
{
   for ( int idx=0; idx<4; idx++ )
       _id[idx] = -1;
}


void RGBALayerProcess::setDataLayerID( int idx, int id, int channel )
{
    if (  idx>=0 && idx<4 )
    {
	_id[idx] = id;
	_textureChannel[idx] = channel>=0 && channel<4 ? channel : 0;
    }
}


int RGBALayerProcess::getDataLayerID( int idx ) const
{ return idx>=0 && idx<4 ? _id[idx] : -1;  } 


int RGBALayerProcess::getDataLayerTextureChannel( int idx ) const
{ return idx>=0 && idx<4 ? _textureChannel[idx] : -1;  } 


void RGBALayerProcess::getShaderCode( std::string& code, const LayeredTexture* layTex, int stage ) const
{
    if ( !layTex )
	return;

    char line[80];
    for ( int idx=0; idx<4; idx++ )
    {
	sprintf( line, "    c%d = vec4( 0.0, 0.0, 0.0, 1.0 );\n", idx );
	code += line;

	if ( !_bilinearFiltering )
	    break;
    }
    code += "\n";

    for ( int idx=0; idx<4; idx++ )
    {
	const int id = getDataLayerID( idx );
	if ( id<0 )
	    continue;

	const int unit = layTex->getDataLayerTextureUnit( id );
	getHeaderCode( code, unit, idx, getDataLayerTextureChannel(idx) );

	if ( _bilinearFiltering )
	{
	    sprintf( line, "    c0[%d] = mix( mix(c0[%d],c1[%d],f0), mix(c2[%d],c3[%d],f0), f1 );\n", idx, idx, idx, idx, idx );
	    code += line;
	}
	code += "\n";
    }

    getFooterCode( code, stage );
}


TransparencyType RGBALayerProcess::getTransparencyType( const LayeredTexture* layTex ) const
{
    if ( _opacity==0.0 )
	return FullyTransparent;

    int id = getDataLayerID(3);
    if ( id<0 )
	return Opaque;

    const osg::Image* image = layTex->getDataLayerImage(id);
    const float borderOpacity = layTex->getDataLayerBorderColor(id).a();
    const int channel = getDataLayerTextureChannel(3);
    const TransparencyType tt = getImageTransparencyType( image, &borderOpacity, channel );

    if ( _opacity==1.0 )
	return tt;

    return tt==FullyTransparent ? tt : HasTransparencies;
}


bool RGBALayerProcess::doProcess( const LayeredTexture*, osg::Image* output )
{ return false; }


IdentityLayerProcess::IdentityLayerProcess( int id )
    : _id( id )
{}


int IdentityLayerProcess::getDataLayerID( int idx ) const
{ return idx ? -1 : _id; } 


void IdentityLayerProcess::getShaderCode( std::string& code, const LayeredTexture* layTex, int stage ) const
{
    if ( !layTex || layTex->getDataLayerIndex(_id)<0 )
	return;

    getHeaderCode( code, layTex->getDataLayerTextureUnit(_id) );

    if ( _bilinearFiltering )
	code += "    c0 = mix( mix(c0,c1,f0), mix(c2,c3,f0), f1 );\n";

    code += "\n";
    getFooterCode( code, stage );
}


TransparencyType IdentityLayerProcess::getTransparencyType( const LayeredTexture* layTex ) const
{
    int id = getDataLayerID();

    if ( id<0 || _opacity==0.0 )
	return FullyTransparent;

    const TransparencyType tt = layTex->getDataLayerTransparencyType(id);

    if ( _opacity==1.0 )
	return tt;

    return tt==FullyTransparent ? tt : HasTransparencies;
}


bool IdentityLayerProcess::doProcess( const LayeredTexture*, osg::Image* output )
{ return false; }


} //namespace
