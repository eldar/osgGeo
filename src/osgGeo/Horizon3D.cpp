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


#include <osg/Geode>
#include <osg/Geometry>

#include <osgGeo/Horizon3D>

#include <iostream>

namespace osgGeo
{

class Horizon3DTesselator : public OpenThreads::Thread
{
public:
    struct CommonData
    {
        CommonData(const Vec2i& fullSize_,
                   osg::DoubleArray *depthVals_,
                   float maxDepth_,
                   const std::vector<osg::Vec2d> &coords_);

        Vec2i fullSize; // full size of the horizon
        osg::DoubleArray *depthVals;
        float maxDepth;
        std::vector<osg::Vec2d> coords;
        Vec2i maxSize; // reference size of the tile
        osg::Vec2d iInc, jInc; // increments of realworld coordinates along the grid dimensions
        int numHTiles, numVTiles; // number of tiles of horizon within
    };

    /**
      * This struct contains information that identifies a tile within horizon
      * and which is used to build it.
      */
    struct Job
    {
        Job(int hI, int vI, int resLevel_) :
            hIdx(hI), vIdx(vI), resLevel(resLevel_) {}

        // indexes of the tile within the horizon
        int hIdx, vIdx;

        // resolution level of horizon 1, 2, 3 ... which means that every
        // first, second, fourth ... points are displayed and all the rest
        // are discarded
        int resLevel;
    };

    struct Result
    {
        osg::ref_ptr<osg::Node> node;
        int resLevel;
    };

    Horizon3DTesselator(const CommonData &data);

    void addJob(const Job &job);
    virtual void run();

    bool isUndef(double val);

    const std::vector<Result> &getResults() const;

private:
    const CommonData &_data;
    std::vector<Job> _jobs;
    std::vector<Result> _results;
};

Horizon3DTesselator::CommonData::CommonData(const Vec2i& fullSize_,
                                            osg::DoubleArray *depthVals_,
                                            float maxDepth_,
                                            const std::vector<osg::Vec2d> &coords_)
{
    fullSize = fullSize_;
    depthVals = depthVals_;
    maxDepth = maxDepth_;
    coords = coords_;

    maxSize = Vec2i(256, 256);

    iInc = (coords[2] - coords[0]) / (fullSize.x() - 1);
    jInc = (coords[1] - coords[0]) / (fullSize.y() - 1);

    numHTiles = ceil(float(fullSize.x()) / maxSize.x());
    numVTiles = ceil(float(fullSize.y()) / maxSize.y());
}

Horizon3DTesselator::Horizon3DTesselator(const CommonData &data) :
    _data(data)
{
}

void Horizon3DTesselator::addJob(const Job &job)
{
    _jobs.push_back(job);
}

bool Horizon3DTesselator::isUndef(double val)
{
    return val >= _data.maxDepth;
}

const std::vector<Horizon3DTesselator::Result> &Horizon3DTesselator::getResults() const
{
    return _results;
}

void Horizon3DTesselator::run()
{
    const CommonData &data = _data;

    for(int jId = 0; jId < _jobs.size(); ++jId)
    {
        const Job &job = _jobs[jId];

        // compression rate. 1 means no compression
        const int compr = pow(2, job.resLevel);
        int iCompr = compr;
        int jCompr = compr;

        int realHSize = data.maxSize.x() / iCompr;
        int realVSize = data.maxSize.y() / jCompr;

        const int hSize = job.hIdx < (data.numHTiles - 1) ?
                    (realHSize + 1) : (data.fullSize.x() - data.maxSize.x() * (data.numHTiles - 1)) / iCompr;
        const int vSize = job.vIdx < (data.numVTiles - 1) ?
                    (realVSize + 1) : ((data.fullSize.y() - data.maxSize.y() * (data.numVTiles - 1))) / jCompr;

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(hSize * vSize);

        // first we construct an array of vertices which is just a grid
        // of depth values.
        for(int i = 0; i < hSize; ++i)
            for(int j = 0; j < vSize; ++j)
            {
                const int iGlobal = job.hIdx * data.maxSize.x() + i * iCompr;
                const int jGlobal = job.vIdx * data.maxSize.y() + j * jCompr;
                osg::Vec2d hor = data.coords[0] + data.iInc * iGlobal + data.jInc * jGlobal;
                (*vertices)[i*vSize+j] = osg::Vec3(
                            hor.x(),
                            hor.y(),
                            data.depthVals->at(iGlobal*data.fullSize.y()+jGlobal)
                            );
            }

        // the following loop populates array of indices that make up
        // triangles out of vertices data, each grid cell has 2 triangles.
        // If a vertex is undefined then triangle that contains it is
        // discarded. Also normals are calculated for each triangle to be
        // later used for calculating normals per vertex
        osg::ref_ptr<osg::DrawElementsUInt> indices =
                new osg::DrawElementsUInt(GL_TRIANGLES);

        osg::ref_ptr<osg::Vec3Array> triangleNormals =
                new osg::Vec3Array((hSize - 1) * (vSize - 1) * 2);

        for(int i = 0; i < hSize - 1; ++i)
            for(int j = 0; j < vSize - 1; ++j)
            {
                const int i00 = i*vSize+j;
                const int i10 = (i+1)*vSize+j;
                const int i01 = i*vSize+(j+1);
                const int i11 = (i+1)*vSize+(j+1);

                const osg::Vec3 v00 = (*vertices)[i00];
                const osg::Vec3 v10 = (*vertices)[i10];
                const osg::Vec3 v01 = (*vertices)[i01];
                const osg::Vec3 v11 = (*vertices)[i11];

                if(isUndef(v10.z()) || isUndef(v01.z()))
                    continue;

                // first triangle
                if(!isUndef(v00.z()))
                {
                    indices->push_back(i00);
                    indices->push_back(i10);
                    indices->push_back(i01);
                }

                // second triangle
                if(!isUndef(v11.z()))
                {
                    indices->push_back(i10);
                    indices->push_back(i01);
                    indices->push_back(i11);
                }

                // calculate triangle normals
                osg::Vec3 norm1 = (v01 - v00) ^ (v10 - v00);
                norm1.normalize();
                (*triangleNormals)[(i*(vSize-1)+j)*2] = norm1;

                osg::Vec3 norm2 = (v10 - v11) ^ (v01 - v11);
                norm2.normalize();
                (*triangleNormals)[(i*(vSize-1)+j)*2+1] = norm2;
            }

        // The following loop calculates normals per vertex. Because
        // each vertex might be shared between many triangles(up to 6)
        // we find out which triangles this particular vertex is shared
        // and then compute the average of normals per triangle.
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(hSize * vSize);

        osg::Vec3 triNormCache[6];

        for(int i = 0; i < hSize; ++i)
            for(int j = 0; j < vSize; ++j)
            {
                int k = 0;
                for(int l = 0; l < k; ++l)
                    triNormCache[l] = osg::Vec3();

                if((i < hSize - 1) && (j < vSize - 1))
                {
                    triNormCache[k++] = (*triangleNormals)[(i*(vSize-1)+j)*2];
                }

                if(i > 0 && j < vSize - 1)
                {
                    triNormCache[k++] = (*triangleNormals)[((i-1)*(vSize-1)+j)*2];
                    triNormCache[k++] = (*triangleNormals)[((i-1)*(vSize-1)+j)*2+1];
                }

                if(j > 0 && i < hSize - 1)
                {
                    triNormCache[k++] = (*triangleNormals)[(i*(vSize-1)+j-1)*2];
                    triNormCache[k++] = (*triangleNormals)[(i*(vSize-1)+j-1)*2+1];
                }

                if(i > 0 && j > 0)
                {
                    triNormCache[k++] = (*triangleNormals)[((i-1)*(vSize-1)+j-1)*2+1];
                }

                if(k > 0)
                {
                    osg::Vec3 norm;
                    for(int l = 0; l < k; ++l)
                        norm += triNormCache[l];

                    norm.normalize();

                    (*normals)[i*vSize+j] = -norm;
                }
            }

        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
        colors->push_back(osg::Vec4(1.0f, 0.0f, 1.0f, 1.0f));

        osg::ref_ptr<osg::Geometry> quad = new osg::Geometry;
        quad->setVertexArray(vertices.get());

        quad->setNormalArray(normals.get());
        quad->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
        quad->setColorArray(colors.get());
        quad->setColorBinding(osg::Geometry::BIND_OVERALL);

        quad->addPrimitiveSet(indices.get());

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(quad.get());

        Result result;
        result.node = geode;
        result.resLevel = job.resLevel;
        _results.push_back(result);
    }
}

Horizon3DNode::Horizon3DNode() :
  Group()
{

}

Horizon3DNode::Horizon3DNode(const Horizon3DNode& other,
                             const osg::CopyOp& op) :
    Group(other, op)
{

}

void Horizon3DNode::setSize(const Vec2i& size)
{
    _size = size;
}

const Vec2i& Horizon3DNode::getSize() const
{
    return _size;
}

void Horizon3DNode::setDepthArray(osg::Array *arr)
{
    _array = arr;
    updateDrawables();
}

const osg::Array *Horizon3DNode::getDepthArray() const
{
    return _array;
}

osg::Array *Horizon3DNode::getDepthArray()
{
    return _array;
}

void Horizon3DNode::setCornerCoords(const std::vector<osg::Vec2d> &coords)
{
    _cornerCoords = coords;
}

std::vector<osg::Vec2d> Horizon3DNode::getCornerCoords() const
{
    return _cornerCoords;
}

void Horizon3DNode::updateDrawables()
{
    if(getDepthArray()->getType() != osg::Array::DoubleArrayType)
        return;

    Horizon3DTesselator::CommonData data(getSize(),
                                         dynamic_cast<osg::DoubleArray*>(getDepthArray()),
                                         getMaxDepth(),
                                         getCornerCoords());

    const int numCPUs = OpenThreads::GetNumberOfProcessors();
    std::vector<Horizon3DTesselator*> threads(numCPUs);
    for(int i = 0; i < numCPUs; ++i)
        threads[i] = new Horizon3DTesselator(data);

    const int resolutionsNum = 3;
    _nodes.resize(resolutionsNum);

    int currentThread = 0;
    for(int resLevel = 0; resLevel < resolutionsNum; ++resLevel)
    {
        for(int hIdx = 0; hIdx < data.numHTiles; ++hIdx)
        {
            for(int vIdx = 0; vIdx < data.numVTiles; ++vIdx)
            {
                threads[currentThread]->addJob(Horizon3DTesselator::Job(hIdx, vIdx, resLevel));
                currentThread++;
                if(currentThread == numCPUs)
                    currentThread = 0;
            }
        }
    }

    for(int i = 0; i < numCPUs; ++i)
        threads[i]->startThread();

    for(int i = 0; i < numCPUs; ++i)
        threads[i]->join();

    for(int i = 0; i < numCPUs; ++i)
    {
        const std::vector<Horizon3DTesselator::Result> &nodes = threads[i]->getResults();
        for(int j = 0; j < nodes.size(); ++j)
        {
            Horizon3DTesselator::Result res = nodes[j];
            addChild(res.node.get());
            _nodes[res.resLevel].push_back(res.node.get());
        }
    }

    for(int i = 0; i < numCPUs; ++i)
        delete threads[i];
}

void Horizon3DNode::setMaxDepth(float val)
{
    _maxdepth = val;
}

float Horizon3DNode::getMaxDepth() const
{
    return _maxdepth;
}

void Horizon3DNode::traverse(osg::NodeVisitor &nv)
{
    float distance = nv.getDistanceToViewPoint(getBound().center(), true);

    const std::vector<osg::Vec2d> coords = getCornerCoords();
    const float iDen = ((coords[2] - coords[0]) / getSize().x()).length();
    const float jDen = ((coords[1] - coords[0]) / getSize().y()).length();

    const float k = std::min(iDen, jDen);
    const float threshold1 = k * 2000.0;
    const float threshold2 = k * 8000.0;

    if(distance < threshold1)
    {
        for(int i = 0; i <  _nodes[0].size(); ++i)
            _nodes[0].at(i)->accept(nv);
    }
    else if(distance < threshold2)
    {
        for(int i = 0; i < _nodes[1].size(); ++i)
            _nodes[1].at(i)->accept(nv);
    }
    else
    {
        for(int i = 0; i < _nodes[2].size(); ++i)
            _nodes[2].at(i)->accept(nv);
    }

}

}
