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

#ifndef HORIZON3D2_H
#define HORIZON3D2_H

#include <osgGeo/Horizon3DBase>

namespace osgGeo
{

class OSGGEO_EXPORT Horizon3DNode2 : public Horizon3DBase
{
public:
    Horizon3DNode2();

protected:
    virtual void updateGeometry();
};

}

#endif // HORIZON3D2_H
