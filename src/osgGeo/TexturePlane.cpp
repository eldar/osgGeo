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


#include <osgGeo/TexturePlane>

#include <osgGeo/LayeredTexture>
#include <osgUtil/CullVisitor>
#include <osg/Geometry>

namespace osgGeo
{

TexturePlaneNode::TexturePlaneNode()
    : _center( 0, 0, 0 )
    , _width( 1, 1, 0 )
    , _textureBrickSize( 64 )
    , _needsUpdate( true )
    , _textureEnvelope( -1, -1 )
    , _disperse( false )
{}


TexturePlaneNode::TexturePlaneNode( const TexturePlaneNode& node, const osg::CopyOp& co )
    : osg::Node( node, co )
    , _center( node._center )
    , _width( node._width )
    , _textureBrickSize( node._textureBrickSize )
    , _needsUpdate( true )
    , _textureEnvelope( -1, -1 )
    , _disperse( node._disperse )
{
    if ( node._texture )
    {
        if ( co.getCopyFlags()==osg::CopyOp::DEEP_COPY_ALL )
	    _texture = static_cast<LayeredTexture*>(node._texture->clone( co ) );
	else
	    _texture = node._texture;
    }
}


TexturePlaneNode::~TexturePlaneNode()
{
    cleanUp();
}


void TexturePlaneNode::cleanUp()
{
    for ( std::vector<osg::Geometry*>::iterator it = _geometries.begin();
	  it!=_geometries.end();
	  it++ )
	(*it)->unref();

    _geometries.clear();

    for ( std::vector<osg::StateSet*>::iterator it = _statesets.begin();
	  it!=_statesets.end();
	  it++ )
	(*it)->unref();

    _statesets.clear();
}


void TexturePlaneNode::traverse( osg::NodeVisitor& nv )
{
    if ( nv.getVisitorType()==osg::NodeVisitor::UPDATE_VISITOR )
    {
	if ( _needsUpdate )
	    updateGeometry();
    }
    else if ( nv.getVisitorType()==osg::NodeVisitor::CULL_VISITOR )
    {
	osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(&nv);

	if ( getStateSet() )
	    cv->pushStateSet( getStateSet() );

	if ( _texture && _texture->getSetupStateSet() )
	    cv->pushStateSet( _texture->getSetupStateSet() );

	for ( int idx=0; idx<_geometries.size(); idx++ )
	{
	    cv->pushStateSet( _statesets[idx] );
	    cv->addDrawable( _geometries[idx], cv->getModelViewMatrix() );
	    cv->popStateSet();
	}

	if ( _texture && _texture->getSetupStateSet() )
	    cv->popStateSet();

	if ( getStateSet() )
	    cv->popStateSet();
    }
}


bool TexturePlaneNode::updateGeometry()
{
    if ( !_texture ) 
	return false;

    cleanUp();

    _textureEnvelope = _texture->getEnvelope();
    const osgGeo::Vec2i texturesize = _textureEnvelope;

    std::vector<int> sOrigins, sSizes;
    _texture->divideAxis( texturesize.x(), _textureBrickSize, sOrigins, sSizes );
    const int nrs = sOrigins.size();
    const int sAxisLen = sOrigins[nrs-1]+sSizes[nrs-1]-1;

    std::vector<int> tOrigins, tSizes;
    _texture->divideAxis( texturesize.y(), _textureBrickSize, tOrigins, tSizes );
    const int nrt = tOrigins.size();
    const int tAxisLen = tOrigins[nrt-1]+tSizes[nrt-1]-1;

    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array;

    const char thinDim = getThinDim();
    const osg::Vec3 normal = thinDim==2 ? osg::Vec3( 0.0f, 0.0f, 1.0f ) :
			     thinDim==1 ? osg::Vec3( 0.0f, 1.0f, 0.0f ) :
					  osg::Vec3( 1.0f, 0.0f, 0.0f ) ;
    normals->push_back( normal );

    for ( int ids=0; ids<nrs; ids++ )
    {
	for ( int idt=0; idt<nrt; idt++ )
	{
	    const float sSize = _disperse ? 0.95*sSizes[ids] : sSizes[ids];
	    const float tSize = _disperse ? 0.95*tSizes[idt] : tSizes[idt];

	    osg::ref_ptr<osg::Vec3Array> coords = new osg::Vec3Array( 4 );

	    (*coords)[0] = osg::Vec3( sOrigins[ids], tOrigins[idt], 0.0f );
	    (*coords)[1] = osg::Vec3( sOrigins[ids]+sSize-1, tOrigins[idt], 0.0f );
	    (*coords)[2] = osg::Vec3( sOrigins[ids]+sSize-1, tOrigins[idt]+tSize-1, 0.0f);
	    (*coords)[3] = osg::Vec3( sOrigins[ids], tOrigins[idt]+tSize-1, 0.0f );

	    for ( int idx=0; idx<4; idx++ )
	    {
		(*coords)[idx].x() /= sAxisLen;
		(*coords)[idx].y() /= tAxisLen;
		(*coords)[idx] -= osg::Vec3( 0.5f, 0.5f, 0.0f );
		(*coords)[idx] *= -1.0f;

		if ( thinDim==0 )
		    (*coords)[idx] = osg::Vec3f( 0.0f, -(*coords)[idx].x(), -(*coords)[idx].y() );
		else if ( thinDim==1 )
		    (*coords)[idx] = osg::Vec3( (*coords)[idx].x(), 0.0f, -(*coords)[idx].y() );


		(*coords)[idx].x() *= _width.x();
		(*coords)[idx].y() *= _width.y();
		(*coords)[idx].z() *= _width.z();
		(*coords)[idx] += _center;
	    }

	    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
	    geometry->ref();

	    geometry->setVertexArray( coords.get() );
	    geometry->setNormalArray( normals.get() );
	    geometry->setNormalBinding( osg::Geometry::BIND_OVERALL );

	    std::vector<LayeredTexture::TextureCoordData> tcData;

	    osgGeo::Vec2i brickOrigin( sOrigins[ids], tOrigins[idt] );
	    osgGeo::Vec2i brickSize( sSizes[ids], tSizes[idt] );
	    osg::ref_ptr<osg::StateSet> stateset = _texture->createCutoutStateSet( brickOrigin, brickSize, tcData );
	    stateset->ref();

	    for ( std::vector<LayeredTexture::TextureCoordData>::iterator it = tcData.begin();
		  it!=tcData.end();
		  it++ )
	    {
		osg::ref_ptr<osg::Vec2Array> tCoords = new osg::Vec2Array( 4 );
		(*tCoords)[0] = it->_tc00;
		(*tCoords)[1] = it->_tc01;
		(*tCoords)[2] = it->_tc11;
		(*tCoords)[3] = it->_tc10;
		geometry->setTexCoordArray( it->_textureUnit, tCoords.get() );
	    }
		    
	    geometry->addPrimitiveSet( new osg::DrawArrays( GL_QUADS, 0, 4 ) );

	    _geometries.push_back( geometry );
	    _statesets.push_back( stateset );
	}
    }

    _needsUpdate = false;
    return true;
}


void TexturePlaneNode::setCenter( const osg::Vec3& center )
{
    _center = center;
    _needsUpdate = true;
}


const osg::Vec3& TexturePlaneNode::getCenter() const
{ return _center; }


void TexturePlaneNode::setTextureBrickSize( short nt )
{
    _textureBrickSize = nt;
}

    
short TexturePlaneNode::getTextureBrickSize() const
{ return _textureBrickSize; }


void TexturePlaneNode::setWidth( const osg::Vec3& width )
{
    _width = width;
    _needsUpdate = true;
}


const osg::Vec3& TexturePlaneNode::getWidth() const
{ return _width; }


void TexturePlaneNode::setLayeredTexture( LayeredTexture* lt )
{
    _texture =  lt;
    _needsUpdate = true;
}


bool TexturePlaneNode::needsUpdate() const
{
    if ( _needsUpdate )
	return true;
    
    return !_texture || _texture->getEnvelope()!=_textureEnvelope;
}


LayeredTexture* TexturePlaneNode::getLayeredTexture()
{ return _texture; }


const LayeredTexture* TexturePlaneNode::getLayeredTexture() const
{ return _texture; }

char TexturePlaneNode::getThinDim() const
{
    if ( !_width.x() )
	return 0;

    if ( !_width.y() )
	return 1;

    return 2;
}



} //namespace osgGeo
