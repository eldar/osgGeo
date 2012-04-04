//      =================================================================
//      |                                                               |
//      |                       COPYRIGHT (C) 2012                      |
//      |               ARK CLS Ltd, Bedford, Bedfordshire, UK          |
//      |                                                               |
//      |                       All Rights Reserved                     |
//      |                                                               |
//      | This software is confidential information which is proprietary|
//      | to and a trade secret of ARK-CLS Ltd. Use, duplication, or    |
//      | disclosure is subject to the terms of a separate source code  |
//      | licence agreement.                                            |
//      |                                                               |
//      =================================================================
//
//

#include "Horizon3DBase"


namespace osgGeo
{

Horizon3DBase::Horizon3DBase()
{
    setNumChildrenRequiringUpdateTraversal(getNumChildrenRequiringUpdateTraversal()+1);
    _needsUpdate = true;
}

Horizon3DBase::Horizon3DBase(const Horizon3DBase& other,
                             const osg::CopyOp& op) :
    osg::Node(other, op)
{
    // TODO Proper copy
}

void Horizon3DBase::setSize(const Vec2i& size)
{
    _size = size;
}

const Vec2i& Horizon3DBase::getSize() const
{
    return _size;
}

void Horizon3DBase::setDepthArray(osg::Array *arr)
{
    _array = arr;
    _needsUpdate = true;
    updateGeometry();
}

const osg::Array *Horizon3DBase::getDepthArray() const
{
    return _array;
}

osg::Array *Horizon3DBase::getDepthArray()
{
    return _array;
}

void Horizon3DBase::setMaxDepth(float val)
{
    _maxDepth = val;
}

float Horizon3DBase::getMaxDepth() const
{
    return _maxDepth;
}

bool Horizon3DBase::isUndef(double val)
{
    return val >= getMaxDepth();
}

void Horizon3DBase::setCornerCoords(const std::vector<osg::Vec2d> &coords)
{
    _cornerCoords = coords;
}

std::vector<osg::Vec2d> Horizon3DBase::getCornerCoords() const
{
    return _cornerCoords;
}

bool Horizon3DBase::needsUpdate() const
{
    return _needsUpdate;
}

void Horizon3DBase::traverse(osg::NodeVisitor &nv)
{
    if ( nv.getVisitorType()==osg::NodeVisitor::UPDATE_VISITOR )
    {
        if ( needsUpdate() )
            updateGeometry();
    }
    else if(nv.getVisitorType()==osg::NodeVisitor::CULL_VISITOR)
    {
        for(int i = 0; i <  _nodes.size(); ++i)
            _nodes.at(i)->accept(nv);
    }
}

}
