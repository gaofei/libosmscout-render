/*
    libosmscout-render

    Copyright (C) 2012, Preet Desai

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "MapRenderer.h"


namespace osmscout
{

MapRenderer::MapRenderer(Database const *myDatabase) :
    m_database(myDatabase),m_dataMinLat(0),m_dataMinLon(0),
    m_dataMaxLon(0),m_dataMaxLat(0)
{
    m_wayNodeCount = 0;

    //
    m_tagBuilding = m_database->GetTypeConfig()->GetTagId("building");
    m_tagHeight = m_database->GetTypeConfig()->GetTagId("height");
}

MapRenderer::~MapRenderer()
{}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::SetRenderStyleConfigs(const std::vector<RenderStyleConfig*> &listStyleConfigs)
{
    // clear old render style configs and save new ones
    m_listRenderStyleConfigs.clear();
    m_listRenderStyleConfigs.resize(listStyleConfigs.size());

    for(int i=0; i < listStyleConfigs.size(); i++)
    {   m_listRenderStyleConfigs[i] = listStyleConfigs[i];   }

    // a new list of render style configs invalidates all
    // current scene data, so we clear everything
    removeAllFromScene();
    m_listWayData.clear();
    m_listAreaData.clear();
    m_listSharedNodes.clear();

    // rebuild all styleConfig related data
    m_listWayData.resize(listStyleConfigs.size());
    m_listAreaData.resize(listStyleConfigs.size());

    for(int i=0; i < listStyleConfigs.size(); i++)
    {
        m_listWayData[i].reserve(350);
        m_listAreaData[i].reserve(200);
    }

    m_listSharedNodes.reserve(5000);
}

void MapRenderer::GetDebugLog(std::vector<std::string> &listDebugMessages)
{
    for(int i=0; i < m_listMessages.size(); i++)
    {   listDebugMessages.push_back(m_listMessages.at(i));   }
}

void MapRenderer::InitializeScene(const PointLLA &camLLA, CameraMode camMode)
{
    if(m_listRenderStyleConfigs.empty())
    {   OSRDEBUG << "ERROR: No render style configs specified!";   return;   }

    // call virtual implementation
    initScene();

    // set camera
    SetCamera(camLLA,camMode);
}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::SetCamera(const PointLLA &camLLA, CameraMode camMode)
{
    Vec3 camNorth,camEast,camDown;
    calcECEFNorthEastDown(camLLA,camNorth,camEast,camDown);

    switch(camMode)
    {
        case CAM_2D:
            break;

        case CAM_ISO_NE:
        {
            camNorth = camNorth.RotatedBy(camDown,45);
            camEast = camEast.RotatedBy(camDown,45);
            camNorth = camNorth.RotatedBy(camEast,90-35.264);
            camDown = camDown.RotatedBy(camEast,90-35.264);
            break;
        }

        default:
            break;
    }

    m_camera.LLA = camLLA;
    m_camera.eye = convLLAToECEF(camLLA);
    m_camera.viewPt = m_camera.eye+camDown.Normalized().ScaledBy(camLLA.alt);
    m_camera.up = camNorth;
    m_camera.fovY = 40;
    m_camera.aspectRatio = 1.33;

    if(!calcCameraViewExtents())
    {   OSRDEBUG << "WARN: Could not calculate view extents";   }
    else
    {   updateSceneBasedOnCamera();   }
}

void MapRenderer::RotateCamera(const Vec3 &axisVec, double angleDegCCW)
{
    // rotate the up vector
    m_camera.up = m_camera.up.RotatedBy(axisVec,angleDegCCW);

    // rotate view dirn/move view point
    m_camera.viewPt = m_camera.eye +
        (m_camera.viewPt-m_camera.eye).RotatedBy(axisVec,angleDegCCW);

    if(!calcCameraViewExtents())
    {
        m_camera.nearDist = 20;
        m_camera.farDist = ELL_SEMI_MAJOR*1.25;
        OSRDEBUG << "WARN: Could not calculate view extents";
        if(m_listWayData.size() > 0)
        {   clearAllRenderData();   }
    }
    else
    {   updateSceneBasedOnCamera();   }
}

void MapRenderer::PanCamera(const Vec3 &dirnVec, double distMeters)
{
    Vec3 moveVec = (dirnVec.Normalized().ScaledBy(distMeters));
    m_camera.eye = m_camera.eye+moveVec;
    m_camera.viewPt = m_camera.viewPt+moveVec;

    if(!calcCameraViewExtents())
    {
        m_camera.nearDist = 20;
        m_camera.farDist = ELL_SEMI_MAJOR*1.25;
        OSRDEBUG << "WARN: Could not calculate view extents";
    }
    else
    {   updateSceneBasedOnCamera();   }
}

void MapRenderer::ZoomCamera(double zoomAmount)
{
    Vec3 viewDirn = (m_camera.viewPt-m_camera.eye).Normalized();
    m_camera.eye = m_camera.eye + viewDirn.ScaledBy(zoomAmount);

    if(!calcCameraViewExtents())
    {
        m_camera.nearDist = 20;
        m_camera.farDist = ELL_SEMI_MAJOR*1.25;
        OSRDEBUG << "WARN: Could not calculate view extents";
    }
    else
    {   updateSceneBasedOnCamera();   }
}

Camera const * MapRenderer::GetCamera()
{   return &m_camera;   }

// ========================================================================== //
// ========================================================================== //

void MapRenderer::updateSceneContents()
{
    // calculate the minimum and maximum distance to
    // m_camera.eye within the available lat/lon bounds
    Vec3 viewBoundsNE;   convLLAToECEF(PointLLA(m_camera.maxLat,m_camera.maxLon),viewBoundsNE);
    Vec3 viewBoundsNW;   convLLAToECEF(PointLLA(m_camera.maxLat,m_camera.minLon),viewBoundsNW);
    Vec3 viewBoundsSW;   convLLAToECEF(PointLLA(m_camera.minLat,m_camera.minLon),viewBoundsSW);
    Vec3 viewBoundsSE;   convLLAToECEF(PointLLA(m_camera.minLat,m_camera.maxLon),viewBoundsSE);

    // to get the minimum distance, find the minima
    // of the minimum distances between m_camera.eye and
    // each edge of the bounding box
    double minDistToNEdge = calcMinPointLineDistance(m_camera.eye,viewBoundsNE,viewBoundsNW);
    double minDistToWEdge = calcMinPointLineDistance(m_camera.eye,viewBoundsNW,viewBoundsSW);
    double minDistToSEdge = calcMinPointLineDistance(m_camera.eye,viewBoundsSW,viewBoundsSE);
    double minDistToEEdge = calcMinPointLineDistance(m_camera.eye,viewBoundsNE,viewBoundsSE);

    double minDistToViewBounds = minDistToNEdge;

    if(minDistToWEdge < minDistToViewBounds)
    {   minDistToViewBounds = minDistToWEdge;   }

    if(minDistToSEdge < minDistToViewBounds)
    {   minDistToViewBounds = minDistToSEdge;   }

    if(minDistToEEdge < minDistToViewBounds)
    {   minDistToViewBounds = minDistToEEdge;   }

    // to get the maximum distance, find the maxima
    // of the distances to each corner of the bounding box
    double distToNE,distToNW,distToSE,distToSW;
    distToNE = m_camera.eye.DistanceTo(viewBoundsNE);
    distToNW = m_camera.eye.DistanceTo(viewBoundsNW);
    distToSE = m_camera.eye.DistanceTo(viewBoundsSE);
    distToSW = m_camera.eye.DistanceTo(viewBoundsSW);

    double maxDistToViewBounds = distToNE;

    if(distToNW > maxDistToViewBounds)
    {   maxDistToViewBounds = distToNW;   }

    if(distToSE > maxDistToViewBounds)
    {   maxDistToViewBounds = distToSE;   }

    if(distToSW > maxDistToViewBounds)
    {   maxDistToViewBounds = distToSW;   }

//    OSRDEBUG << "INFO: minDistToViewBounds: " << minDistToViewBounds;
//    OSRDEBUG << "INFO: maxDistToViewBounds: " << maxDistToViewBounds;

    // use the min and max distance between m_camera.eye
    // and the view bounds to set active LOD ranges
    unsigned int numLodRanges = m_listRenderStyleConfigs.size();
    std::vector<bool> listLODRangesActive(numLodRanges);
    std::vector<std::pair<double,double> > listLODRanges(numLodRanges);

    for(int i=0; i < numLodRanges; i++)
    {
        std::pair<double,double> lodRange;
        lodRange.first = m_listRenderStyleConfigs.at(i)->GetMinDistance();
        lodRange.second = m_listRenderStyleConfigs.at(i)->GetMaxDistance();
        listLODRanges[i] = lodRange;

        // if the min-max distance range overlaps with
        // lodRange, set the range as active
        if(lodRange.second < minDistToViewBounds ||
           lodRange.first > maxDistToViewBounds)
        {   listLODRangesActive[i] = false;   }
        else
        {   listLODRangesActive[i] = true;   }
    }

    // check if at least one valid style
    bool hasValidStyle = false;
    for(int i=0; i < listLODRangesActive.size(); i++)
    {
        if(listLODRangesActive[i])
        {   hasValidStyle = true;   break;   }
    }

    if(!hasValidStyle)
    {   OSRDEBUG << "WARN: No valid style data found";   return;   }

    // for all ranges that are active, get the overlap
    // of the range extents with the view extents to
    // define a bounding box for the database query
    PointLLA camLLA;
    convECEFToLLA(m_camera.eye,camLLA);

    unsigned int numRanges = listLODRanges.size();
    std::vector<std::unordered_map<Id,WayRef> >   listWayRefsByLod(numRanges);
    std::vector<std::unordered_map<Id,WayRef> >   listAreaRefsByLod(numRanges);

    std::unordered_map<Id,WayRefAndLod> listWayRefsAllLods(600);
    std::unordered_map<Id,WayRefAndLod> listAreaRefsAllLods(300);


    for(int i=0; i < numRanges; i++)
    {
        if(listLODRangesActive[i])
        {
            // get range extents based on camera and lodRange
            PointLLA rangeN,rangeE,rangeS,rangeW;
            calcGeographicDestination(camLLA,0,listLODRanges[i].second,rangeN);
            calcGeographicDestination(camLLA,90,listLODRanges[i].second,rangeE);
            calcGeographicDestination(camLLA,180,listLODRanges[i].second,rangeS);
            calcGeographicDestination(camLLA,270,listLODRanges[i].second,rangeW);

            // check if range and view extents intersect
            if((m_camera.maxLon < rangeW.lon) || (m_camera.minLon > rangeE.lon) ||
               (m_camera.maxLat < rangeS.lat) || (m_camera.minLat > rangeN.lat))
            {   continue;   }

            // get intersection rectangle
            double queryMinLon = std::max(m_camera.minLon,rangeW.lon);
            double queryMinLat = std::max(m_camera.minLat,rangeS.lat);
            double queryMaxLon = std::min(m_camera.maxLon,rangeE.lon);
            double queryMaxLat = std::min(m_camera.maxLat,rangeN.lat);

            // get objects from database
            std::vector<TypeId> listTypeIds;
            m_listRenderStyleConfigs.at(i)->GetActiveTypes(listTypeIds);

            std::vector<NodeRef>        listNodeRefs;
            std::vector<WayRef>         listWayRefs;
            std::vector<WayRef>         listAreaRefs;
            std::vector<RelationRef>    listRelWayRefs;
            std::vector<RelationRef>    listRelAreaRefs;

            if(m_database->GetObjects(queryMinLon,queryMinLat,
                                      queryMaxLon,queryMaxLat,
                                      listTypeIds,
                                      listNodeRefs,
                                      listWayRefs,
                                      listAreaRefs,
                                      listRelWayRefs,
                                      listRelAreaRefs))
            {
                // we retrieve objects from a high LOD (close up zoom)
                // to a lower LOD (far away zoom)

                // since the database query does not have finite resolution,
                // we cull all results that have already been retrieved for
                // all previous LOD ranges to prevent duplicates by first
                // inserting into a 'parent' map<Id,WayRefAndLod> before
                // saving into the 'sub' map<Id,WayRef> organzied by lod

                // WAYS
                std::vector<WayRef>::iterator wayIt;
                for(wayIt = listWayRefs.begin();
                    wayIt != listWayRefs.end();)
                {
                    WayRefAndLod wayRefLod(*wayIt,i);
                    std::pair<Id,WayRefAndLod> insWay((*wayIt)->GetId(),wayRefLod);

                    if(listWayRefsAllLods.insert(insWay).second)
                    {   listWayRefsByLod[i].insert(std::make_pair((*wayIt)->GetId(),*wayIt));   }

                    ++wayIt;
                }

                // AREAS
                std::vector<WayRef>::iterator areaIt;
                for(areaIt = listAreaRefs.begin();
                    areaIt != listAreaRefs.end();)
                {
                    WayRefAndLod areaRefLod(*areaIt,i);
                    std::pair<Id,WayRefAndLod> insArea((*areaIt)->GetId(),areaRefLod);

                    if(listAreaRefsAllLods.insert(insArea).second)
                    {   listAreaRefsByLod[i].insert(std::make_pair((*areaIt)->GetId(),*areaIt));   }

                    ++areaIt;
                }
            }
        }
    }

    updateWayRenderData(listWayRefsByLod);
    updateAreaRenderData(listAreaRefsByLod);

    // update current data extents
    m_dataMinLat = m_camera.minLat;
    m_dataMinLon = m_camera.minLon;
    m_dataMaxLat = m_camera.maxLat;
    m_dataMaxLon = m_camera.maxLon;
}

void MapRenderer::updateSceneBasedOnCamera()
{
    double oldArea = (m_dataMaxLat-m_dataMinLat)*
                     (m_dataMaxLon-m_dataMinLon);

    double newArea = (m_camera.maxLat-m_camera.minLat)*
                     (m_camera.maxLon-m_camera.minLon);

    double overlapArea = calcAreaRectOverlap(m_camera.minLon,m_camera.minLat,
                                             m_camera.maxLon,m_camera.maxLat,
                                             m_dataMinLon,m_dataMinLat,
                                             m_dataMaxLon,m_dataMaxLat);

    if(oldArea < 1E-7)
    {   updateSceneContents();   return;   }

    double oldOverlap = overlapArea/oldArea;
    double newOverlap = overlapArea/newArea;

    if(oldOverlap < 0.75 || newOverlap < 0.75)
    {
        OSRDEBUG << "INFO: [Updating Scene Contents...]";
        updateSceneContents();
    }
}

bool MapRenderer::calcCameraViewExtents()
{
    return calcCameraViewExtents(m_camera.eye,m_camera.viewPt,m_camera.up,
                                 m_camera.fovY,m_camera.aspectRatio,
                                 m_camera.nearDist,m_camera.farDist,
                                 m_camera.minLat,m_camera.maxLat,
                                 m_camera.minLon,m_camera.maxLon);
}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::updateWayRenderData(std::vector<std::unordered_map<Id,WayRef> > &listWayRefsByLod)
{
    int thingsAdded = 0;
    int thingsRemoved = 0;

//    OSRDEBUG << "INFO:    Shared Nodes Before Update: " << m_listSharedNodes.size();

    for(int i=0; i < listWayRefsByLod.size(); i++)
    {
        std::unordered_map<Id,WayRef>::iterator itNew;
        std::unordered_map<Id,WayRenderData>::iterator itOld;

        // remove objects from the old view extents
        // not present in the new view extents
        for(itOld = m_listWayData[i].begin();
            itOld != m_listWayData[i].end();)
        {
            itNew = listWayRefsByLod[i].find((*itOld).first);

            if(itNew == listWayRefsByLod[i].end())
            {   // way dne in new view -- remove it
                std::unordered_map<Id,WayRenderData>::iterator itDelete = itOld;

                removeWayFromSharedNodes(itDelete->second.wayRef);
                removeWayFromScene((*itDelete).second); ++itOld;
                m_listWayData[i].erase(itDelete);
                thingsRemoved++;
            }
            else
            {   ++itOld;   }
        }

        // add objects from the new view extents
        // not present in the old view extents
        WayRenderData wayRenderData;
        for(itNew = listWayRefsByLod[i].begin();
            itNew != listWayRefsByLod[i].end(); ++itNew)
        {
            itOld = m_listWayData[i].find((*itNew).first);

            if(itOld == m_listWayData[i].end())
            {   // way dne in old view -- add it
                if(genWayRenderData((*itNew).second,
                                    m_listRenderStyleConfigs[i],
                                    wayRenderData))
                {
                    addWayToScene(wayRenderData);
                    std::pair<Id,WayRenderData> insPair((*itNew).first,wayRenderData);
                    m_listWayData[i].insert(insPair);
                    thingsAdded++;
                }
            }
        }
    }

//    OSRDEBUG << "INFO:    Ways Removed: " << thingsRemoved;
//    OSRDEBUG << "INFO:    Ways Added: " << thingsAdded;
//    OSRDEBUG << "INFO:    Shared Nodes After Update: " << m_listSharedNodes.size();
}

void MapRenderer::updateAreaRenderData(std::vector<std::unordered_map<Id,WayRef> > &listAreaRefsByLod)
{
    for(int i=0; i < listAreaRefsByLod.size(); i++)
    {
        std::unordered_map<Id,WayRef>::iterator itNew;
        std::unordered_map<Id,AreaRenderData>::iterator itOld;

        // remove objects from the old view extents
        // not present in the new view extents
        for(itOld = m_listAreaData[i].begin();
            itOld != m_listAreaData[i].end();)
        {
            itNew = listAreaRefsByLod[i].find((*itOld).first);

            if(itNew == listAreaRefsByLod[i].end())
            {   // way dne in new view -- remove it              
                std::unordered_map<Id,AreaRenderData>::iterator itDelete = itOld;
                removeAreaFromScene((*itDelete).second); ++itOld;
                m_listAreaData[i].erase(itDelete);
            }
            else
            {   ++itOld;   }
        }

        // add objects from the new view extents
        // not present in the old view extents
        for(itNew = listAreaRefsByLod[i].begin();
            itNew != listAreaRefsByLod[i].end(); ++itNew)
        {
            itOld = m_listAreaData[i].find((*itNew).first);

            if(itOld == m_listAreaData[i].end())
            {   // way dne in old view -- add it
                AreaRenderData areaRenderData;

                if(genAreaRenderData((*itNew).second,
                                     m_listRenderStyleConfigs[i],
                                     areaRenderData))
                {
                    addAreaToScene(areaRenderData);
                    std::pair<Id,AreaRenderData> insPair((*itNew).first,areaRenderData);
                    m_listAreaData[i].insert(insPair);
                }
            }
        }
    }
}

// ========================================================================== //
// ========================================================================== //

bool MapRenderer::genAreaRenderData(const WayRef &areaRef,
                                    const RenderStyleConfig *renderStyle,
                                    AreaRenderData &areaRenderData)
{
    // ensure that the area is defined by min. 3 points
    if(areaRef->nodes.size() < 3)
    {
        OSRDEBUG << "WARN: AreaRef " << areaRef->GetId()
                 << " has less than 3 points";
        return false;
    }

    // ensure that the poly is simple (no intersecting edges)
    // before building the area geometry in ecef coordinates
    // TODO: this won't work at +/- 180 deg latitude
    std::vector<osmscout::Vec2> listGeoPoints(areaRef->nodes.size());
    for(int i=0; i < listGeoPoints.size(); i++)
    {
        listGeoPoints[i].x = areaRef->nodes[i].GetLon();
        listGeoPoints[i].y = areaRef->nodes[i].GetLat();
    }

    if(!calcPolyIsSimple(listGeoPoints))
    {
        OSRDEBUG << "WARN: AreaRef " << areaRef->GetId()
                 << " is a complex polygon";
        return false;
    }

    TypeId areaType = areaRef->GetType();


    // check if area is a building
    areaRenderData.isBuilding = false;
    double areaHeight = 0;
    if(areaRef->GetTagCount() > 0)
    {
        for(int i=0; i < areaRef->GetTagCount(); i++)
        {
            if(areaRef->GetTagKey(i) == m_tagBuilding)
            {
                std::string keyVal = areaRef->GetTagValue(i);
                if(keyVal == "yes" || keyVal == "true" || keyVal == "1")
                {   areaRenderData.isBuilding = true;   }
            }

            else if(areaRef->GetTagKey(i) == m_tagHeight)
            {   areaHeight = convStrToDbl(areaRef->GetTagValue(i));   }
        }
    }

    // set area data
    areaRenderData.areaRef = areaRef;
    areaRenderData.areaLayer = 1;   // TODO
    areaRenderData.fillRenderStyle =
            renderStyle->GetAreaFillRenderStyle(areaType);

    if(areaRenderData.isBuilding)
    {
        areaRenderData.buildingData = new BuildingData;
        areaRenderData.buildingData->height =
                (areaHeight > 0) ? areaHeight : 10;
    }

    // get poly orientation
    // TODO: this won't work at +/- 180 deg latitude
    areaRenderData.pathIsCCW = calcPolyIsCCW(listGeoPoints);

    // convert area geometry to ecef
    double layerElevation = areaRenderData.areaLayer*0.05;
    areaRenderData.listBorderPoints.resize(areaRef->nodes.size());
    for(int i=0; i < areaRenderData.listBorderPoints.size(); i++)
    {
        areaRenderData.listBorderPoints[i] =
                convLLAToECEF(PointLLA(areaRef->nodes[i].GetLat(),
                                       areaRef->nodes[i].GetLon(),
                                       layerElevation));
    }

    // save center point
    double centerLat,centerLon;
    areaRef->GetCenter(centerLat,centerLon);
    areaRenderData.centerPoint =
            convLLAToECEF(PointLLA(centerLat,
                                   centerLon,
                                   layerElevation));

    // set area label
    areaRenderData.nameLabel = areaRef->GetName();
    areaRenderData.nameLabelRenderStyle =
            renderStyle->GetAreaNameLabelRenderStyle(areaType);

    areaRenderData.hasName = (areaRenderData.nameLabel.size() > 0) &&
            !(areaRenderData.nameLabelRenderStyle == NULL);

    return true;
}

bool MapRenderer::genWayRenderData(const WayRef &wayRef,
                                   const RenderStyleConfig *renderStyle,
                                   WayRenderData &wayRenderData)
{
    TypeId wayType = wayRef->GetType();

    // set general way properties
    wayRenderData.wayRef = wayRef;
    wayRenderData.wayLayer = renderStyle->GetWayLayer(wayType);
    wayRenderData.lineRenderStyle =
            renderStyle->GetWayLineRenderStyle(wayType);

    // build way geometry
    wayRenderData.listWayPoints.resize(wayRef->nodes.size());
    wayRenderData.listSharedNodes.resize(wayRef->nodes.size());
    for(int i=0; i < wayRef->nodes.size(); i++)
    {
        wayRenderData.listWayPoints[i] =
                convLLAToECEF(PointLLA(wayRef->nodes[i].GetLat(),
                                       wayRef->nodes[i].GetLon(),
                                       wayRenderData.wayLayer * 0.05));

        if(m_listSharedNodes.count(wayRef->nodes[i].GetId()))
        {   wayRenderData.listSharedNodes[i] = true;    }
        else
        {   wayRenderData.listSharedNodes[i] = false;   }

        std::pair<Id,Id> nodeInWay(wayRef->nodes[i].GetId(),wayRef->GetId());
        m_listSharedNodes.insert(nodeInWay);

        m_wayNodeCount++;
    }

    // way label data
    wayRenderData.nameLabel = wayRef->GetName();
    wayRenderData.nameLabelRenderStyle =
            renderStyle->GetWayNameLabelRenderStyle(wayType);

    wayRenderData.hasName = (wayRenderData.nameLabel.size() > 0) &&
            !(wayRenderData.nameLabelRenderStyle == NULL);

    return true;
}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::removeWayFromSharedNodes(const WayRef &wayRef)
{
    std::unordered_map<Id,Id>::iterator itShNode;
    std::pair<std::unordered_map<Id,Id>::iterator,
              std::unordered_map<Id,Id>::iterator> itRange;

    for(int i=0; i < wayRef->nodes.size(); i++)
    {
        itRange = m_listSharedNodes.equal_range(wayRef->nodes[i].GetId());

        for(itShNode = itRange.first;
            itShNode != itRange.second;)
        {
            if(itShNode->second == wayRef->GetId())
            {   m_listSharedNodes.erase(itShNode);  break;   }

            ++itShNode;
        }
    }
}

void MapRenderer::clearAllRenderData()
{
    for(int i=0; i < m_listRenderStyleConfigs.size(); i++)
    {
        m_listWayData[i].clear();
        m_listAreaData[i].clear();
    }

    removeAllFromScene();
}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::convLLAToECEF(const PointLLA &pointLLA, Vec3 &pointECEF)
{
    // conversion formula from...
    // hxxp://www.microem.ru/pages/u_blox/tech/dataconvert/GPS.G1-X-00006.pdf

    // remember to convert deg->rad
    double sinLat = sin(pointLLA.lat * K_PI/180.0f);
    double sinLon = sin(pointLLA.lon * K_PI/180.0f);
    double cosLat = cos(pointLLA.lat * K_PI/180.0f);
    double cosLon = cos(pointLLA.lon * K_PI/180.0f);

    // v = radius of curvature (meters)
    double v = ELL_SEMI_MAJOR / (sqrt(1-(ELL_ECC_EXP2*sinLat*sinLat)));
    pointECEF.x = (v + pointLLA.alt) * cosLat * cosLon;
    pointECEF.y = (v + pointLLA.alt) * cosLat * sinLon;
    pointECEF.z = ((1-ELL_ECC_EXP2)*v + pointLLA.alt)*sinLat;
}

Vec3 MapRenderer::convLLAToECEF(const PointLLA &pointLLA)
{
    Vec3 pointECEF;

    // remember to convert deg->rad
    double sinLat = sin(pointLLA.lat * K_PI/180.0f);
    double sinLon = sin(pointLLA.lon * K_PI/180.0f);
    double cosLat = cos(pointLLA.lat * K_PI/180.0f);
    double cosLon = cos(pointLLA.lon * K_PI/180.0f);

    // v = radius of curvature (meters)
    double v = ELL_SEMI_MAJOR / (sqrt(1-(ELL_ECC_EXP2*sinLat*sinLat)));
    pointECEF.x = (v + pointLLA.alt) * cosLat * cosLon;
    pointECEF.y = (v + pointLLA.alt) * cosLat * sinLon;
    pointECEF.z = ((1-ELL_ECC_EXP2)*v + pointLLA.alt)*sinLat;

    return pointECEF;
}

void MapRenderer::convECEFToLLA(const Vec3 &pointECEF, PointLLA &pointLLA)
{
    // conversion formula from...
    // hxxp://www.microem.ru/pages/u_blox/tech/dataconvert/GPS.G1-X-00006.pdf

    double p = (sqrt(pow(pointECEF.x,2) + pow(pointECEF.y,2)));
    double th = atan2(pointECEF.z*ELL_SEMI_MAJOR, p*ELL_SEMI_MINOR);
    double sinTh = sin(th);
    double cosTh = cos(th);

    // calc longitude
    pointLLA.lon = atan2(pointECEF.y, pointECEF.x);

    // calc latitude
    pointLLA.lat = atan2(pointECEF.z + ELL_ECC2_EXP2*ELL_SEMI_MINOR*sinTh*sinTh*sinTh,
                         p - ELL_ECC_EXP2*ELL_SEMI_MAJOR*cosTh*cosTh*cosTh);
    // calc altitude
    double sinLat = sin(pointLLA.lat);
    double N = ELL_SEMI_MAJOR / (sqrt(1-(ELL_ECC_EXP2*sinLat*sinLat)));
    pointLLA.alt = (p/cos(pointLLA.lat)) - N;

    // convert from rad to deg
    pointLLA.lon = pointLLA.lon * 180.0/K_PI;
    pointLLA.lat = pointLLA.lat * 180.0/K_PI;
}

PointLLA MapRenderer::convECEFToLLA(const Vec3 &pointECEF)
{
    PointLLA pointLLA;

    double p = (sqrt(pow(pointECEF.x,2) + pow(pointECEF.y,2)));
    double th = atan2(pointECEF.z*ELL_SEMI_MAJOR, p*ELL_SEMI_MINOR);
    double sinTh = sin(th);
    double cosTh = cos(th);

    // calc longitude
    pointLLA.lon = atan2(pointECEF.y, pointECEF.x);

    // calc latitude
    pointLLA.lat = atan2(pointECEF.z + ELL_ECC2_EXP2*ELL_SEMI_MINOR*sinTh*sinTh*sinTh,
                         p - ELL_ECC_EXP2*ELL_SEMI_MAJOR*cosTh*cosTh*cosTh);
    // calc altitude
    double sinLat = sin(pointLLA.lat);
    double N = ELL_SEMI_MAJOR / (sqrt(1-(ELL_ECC_EXP2*sinLat*sinLat)));
    pointLLA.alt = (p/cos(pointLLA.lat)) - N;

    // convert from rad to deg
    pointLLA.lon = pointLLA.lon * 180.0/K_PI;
    pointLLA.lat = pointLLA.lat * 180.0/K_PI;

    return pointLLA;
}

double MapRenderer::convStrToDbl(const std::string &strNum)
{
    std::istringstream iss(strNum);
    double numVal;

    if(!(iss >> numVal))
    {   numVal=0;   }

    return numVal;
}

void MapRenderer::calcECEFNorthEastDown(const PointLLA &pointLLA,
                                        Vec3 &vecNorth,
                                        Vec3 &vecEast,
                                        Vec3 &vecDown)
{
    Vec3 pOrigin = convLLAToECEF(pointLLA);

    Vec3 slightlyNorth = convLLAToECEF(PointLLA(pointLLA.lat+0.000001,
                                                pointLLA.lon,
                                                pointLLA.alt));

    Vec3 slightlyEast = convLLAToECEF(PointLLA(pointLLA.lat,
                                               pointLLA.lon+0.000001,
                                               pointLLA.alt));

    Vec3 slightlyDown = convLLAToECEF(PointLLA(pointLLA.lat,
                                               pointLLA.lon,
                                               pointLLA.alt-50));

    vecNorth = (slightlyNorth-pOrigin).Normalized();
    vecEast = (slightlyEast-pOrigin).Normalized();
    vecDown = (slightlyDown-pOrigin).Normalized();
}

void MapRenderer::calcQuadraticEquationReal(double a, double b, double c,
                                            std::vector<double> &listRoots)
{
    // check discriminant
    double myDiscriminant = b*b - 4*a*c;

    if(myDiscriminant > 0)
    {
        double qSeg1 = (-1*b)/(2*a);
        double qSeg2 = sqrt(myDiscriminant)/(2*a);
        listRoots.push_back(qSeg1+qSeg2);
        listRoots.push_back(qSeg1-qSeg2);
    }
}

bool MapRenderer::calcLinesIntersect(double a_x1, double a_y1,
                                     double a_x2, double a_y2,
                                     double b_x1, double b_y1,
                                     double b_x2, double b_y2)
{
    double ua_numr = (b_x2-b_x1)*(a_y1-b_y1)-(b_y2-b_y1)*(a_x1-b_x1);
    double ub_numr = (a_x2-a_x1)*(a_y1-b_y1)-(a_y2-a_y1)*(a_x1-b_x1);
    double denr = (b_y2-b_y1)*(a_x2-a_x1)-(b_x2-b_x1)*(a_y2-a_y1);

    if(denr == 0.0)
    {
        // lines are coincident
        if(ua_numr == 0.0 && ub_numr == 0.0)
        {   return true;   }

        // lines are parallel
        else
        {   return false;   }
    }

    double ua = ua_numr/denr;
    double ub = ub_numr/denr;

    if(ua >= 0.0 && ua <= 1.0 && ub >= 0.0 && ub <= 1.0)
    {   return true;   }

    return false;
}

bool MapRenderer::calcPolyIsSimple(const std::vector<Vec2> &listPolyPoints)
{
    // test poly by starting with a given edge, and
    // checking whether or not it intersects with any
    // other edges in the polygon

    // this is done naively O(n^2) -- better
    // implementation would be a line sweep algorithm

    std::vector<Vec2> listPoints(listPolyPoints.size()+1);
    for(int i=0; i < listPoints.size()-1; i++)
    {   listPoints[i] = listPolyPoints[i];   }
    listPoints.back() = listPolyPoints[0];

    for(int i=0; i < listPoints.size()-1; i++)
    {
        unsigned int edgesIntersect = 0;
        for(int j=i+1; j < listPoints.size()-1; j++)
        {
            if(calcLinesIntersect(listPoints[i].x,
                                  listPoints[i].y,
                                  listPoints[i+1].x,
                                  listPoints[i+1].y,
                                  listPoints[j].x,
                                  listPoints[j].y,
                                  listPoints[j+1].x,
                                  listPoints[j+1].y))
            {
                edgesIntersect++;

                // when i == 0, we check the first edge against every
                // other edge and expect to see 2 intersections for
                // adjacent edges; poly is complex if there are more
                // intersections

                if(i == 0)
                {
                    if(edgesIntersect > 2)
                    {   return false;   }
                }

                // when i != 0 we check an edge that isn't the first
                // edge against every other edge excluding those that
                // have already been tested (this means one adjacent
                // edge); poly is complex if there is more than one
                // intersection

                else
                {
                    if(edgesIntersect > 1)
                    {   return false;   }
                }
            }
        }
    }
    return true;
}

bool MapRenderer::calcPolyIsCCW(const std::vector<Vec2> &listPoints)
{
    // based on  hxxp://en.wikipedia.org/wiki/Curve_orientation
    // and       hxxp://local.wasp.uwa.edu.au/~pbourke/geometry/clockwise/

    // note: poly must be simple

    int ptIdx = 0;
    for(int i=1; i < listPoints.size(); i++)
    {
        // find the point with the smallest y value,
        if(listPoints[i].y < listPoints[ptIdx].y)
        {   ptIdx = i;   }

        // if y values are equal save the point with greatest x
        else if(listPoints[i].y == listPoints[ptIdx].y)
        {
            if(listPoints[i].x < listPoints[ptIdx].x)
            {   ptIdx = i;   }
        }
    }

    int prevIdx = (ptIdx == 0) ? listPoints.size()-1 : ptIdx-1;
    int nextIdx = (ptIdx == listPoints.size()-1) ? 0 : ptIdx+1;

    double signedArea = (listPoints[ptIdx].x-listPoints[prevIdx].x) *
                        (listPoints[nextIdx].y-listPoints[ptIdx].y) -
                        (listPoints[ptIdx].y-listPoints[prevIdx].y) *
                        (listPoints[nextIdx].x-listPoints[ptIdx].x);

    return (signedArea > 0.0);
}

double MapRenderer::calcAreaRectOverlap(double r1_bl_x, double r1_bl_y,
                                        double r1_tr_x, double r1_tr_y,
                                        double r2_bl_x, double r2_bl_y,
                                        double r2_tr_x, double r2_tr_y)
{
    // check if rectangles intersect
    if((r1_tr_x < r2_bl_x) || (r1_bl_x > r2_tr_x) ||
            (r1_tr_y < r2_bl_y) || (r1_bl_y > r2_tr_y))
    {   return 0;   }

    double r3_bl_x = std::max(r1_bl_x,r2_bl_x);
    double r3_bl_y = std::max(r2_bl_y,r2_bl_y);
    double r3_tr_x = std::min(r1_tr_x,r2_tr_x);
    double r3_tr_y = std::min(r2_tr_y,r2_tr_y);

    return ((r3_tr_x-r3_bl_x)*(r3_tr_y-r3_bl_y));
}

double MapRenderer::calcMinPointLineDistance(const Vec3 &distalPoint,
                                             const Vec3 &endPointA,
                                             const Vec3 &endPointB)
{   // ref: http://paulbourke.net/geometry/pointline/

    Vec3 lineSegment = endPointB-endPointA;
    double uNumr = distalPoint.Dot(lineSegment)-endPointA.Dot(lineSegment);
    double uDenr = (lineSegment).Dot(lineSegment);

    if(uDenr == 0)
    {   return distalPoint.DistanceTo(endPointA);   }

    double u = uNumr/uDenr;

    // check whether or not the projection falls
    // within the line segment
    if(u < 0)
    {   return distalPoint.DistanceTo(endPointA);   }

    else if(u > 1)
    {   return distalPoint.DistanceTo(endPointB);   }

    else
    {   return distalPoint.DistanceTo(endPointA+lineSegment.ScaledBy(u));   }
}

double MapRenderer::calcMaxPointLineDistance(const Vec3 &distalPoint,
                                             const Vec3 &endPointA,
                                             const Vec3 &endPointB)
{
    double distA = distalPoint.DistanceTo(endPointA);
    double distB = distalPoint.DistanceTo(endPointB);
    return std::max(distA,distB);
}

double MapRenderer::calcMinPointPlaneDistance(const Vec3 &distalPoint,
                                              const Vec3 &planePoint,
                                              const Vec3 &planeNormal)
{
    // ref: http://paulbourke.net/geometry/pointline/

    // find the plane's coeffecients
    double a = planeNormal.x;
    double b = planeNormal.y;
    double c = planeNormal.z;
    double d = -1 * (a*planePoint.x + b*planePoint.y + c*planePoint.z);

    double distance = (a*distalPoint.x + b*distalPoint.y + c*distalPoint.z + d) /
        sqrt(a*a + b*b + c*c);

    return fabs(distance);
}

bool MapRenderer::calcGeographicDestination(const PointLLA &pointStart,
                                            double bearingDegrees,
                                            double distanceMeters,
                                            PointLLA &pointDest)
{
    // ref: http://www.movable-type.co.uk/scripts/latlong.html

    double bearingRad = bearingDegrees * K_PI/180.0;
    double angularDist = distanceMeters / ELL_SEMI_MAJOR;
    double lat1 = pointStart.lat * K_PI/180.0;
    double lon1 = pointStart.lon * K_PI/180.0;

    pointDest.lat = asin(sin(lat1)*cos(angularDist) +
                         cos(lat1)*sin(angularDist)*cos(bearingRad));

    pointDest.lon = lon1 + atan2(sin(bearingRad)*sin(angularDist)*cos(lat1),
                          cos(angularDist)-sin(lat1)*sin(pointDest.lat));

    // convert back to degrees
    pointDest.lat *= (180.0/K_PI);
    pointDest.lon *= (180.0/K_PI);
}

bool MapRenderer::calcPointLiesAlongRay(const Vec3 &distalPoint,
                                        const Vec3 &rayPoint,
                                        const Vec3 &rayDirn)
{
    double u = (distalPoint.x-rayPoint.x)/rayDirn.x;
    if(u >= 0)
    {
        if(distalPoint.y == (rayPoint.y + u*rayDirn.y))
        {
            if(distalPoint.z == (rayPoint.z + u*rayDirn.z))
            {
                return true;
            }
        }
    }

    return false;
}

bool MapRenderer::calcRayPlaneIntersection(const Vec3 &linePoint,
                                           const Vec3 &lineDirn,
                                           const Vec3 &planePoint,
                                           const Vec3 &planeNormal,
                                           Vec3 &intersectionPoint)
{
    // ref: http://paulbourke.net/geometry/planeline/

    // find the plane's coeffecients
    double a = planeNormal.x;
    double b = planeNormal.y;
    double c = planeNormal.z;
    double d = -1 * (a*planePoint.x + b*planePoint.y + c*planePoint.z);

    // solve for the point of intersection
    Vec3 lineOtherPoint = linePoint + lineDirn;
    double uNumr = a*linePoint.x + b*linePoint.y + c*linePoint.z + d;
    double uDenmr = a*(linePoint.x - lineOtherPoint.x) +
                    b*(linePoint.y-lineOtherPoint.y) +
                    c*(linePoint.z-lineOtherPoint.z);

    if(uDenmr == 0)           // ray is || to plane
    {   return false;   }

    if((uNumr/uDenmr) < 0)    // poi lies along opposite ray dirn
    {   return false;   }

    intersectionPoint = linePoint + lineDirn.ScaledBy(uNumr/uDenmr);
    return true;
}

bool MapRenderer::calcRayEarthIntersection(const Vec3 &linePoint,
                                           const Vec3 &lineDirn,
                                           Vec3 &nearXsecPoint)
{
    // the solution for intersection points between a ray
    // and the Earth's surface is a quadratic equation

    // first calculate the quadratic equation params:
    // a(x^2) + b(x) + c

    // a, b and c are found by substituting the parametric
    // equation of a line into the equation for a ellipsoid
    // and solving in terms of the line's parameter

    // * http://en.wikipedia.org/wiki/Ellipsoid
    // * http://gis.stackexchange.com/questions/20780/...
    //   ...point-of-intersection-for-a-ray-and-earths-surface

    std::vector<double> listRoots;

    double a = (pow(lineDirn.x,2) / ELL_SEMI_MAJOR_EXP2) +
               (pow(lineDirn.y,2) / ELL_SEMI_MAJOR_EXP2) +
               (pow(lineDirn.z,2) / ELL_SEMI_MINOR_EXP2);

    double b = (2*linePoint.x*lineDirn.x/ELL_SEMI_MAJOR_EXP2) +
               (2*linePoint.y*lineDirn.y/ELL_SEMI_MAJOR_EXP2) +
               (2*linePoint.z*lineDirn.z/ELL_SEMI_MINOR_EXP2);

    double c = (pow(linePoint.x,2) / ELL_SEMI_MAJOR_EXP2) +
               (pow(linePoint.y,2) / ELL_SEMI_MAJOR_EXP2) +
               (pow(linePoint.z,2) / ELL_SEMI_MINOR_EXP2) - 1;

    calcQuadraticEquationReal(a,b,c,listRoots);
    if(!listRoots.empty())
    {   // ensure poi lies along ray dirn
        if((listRoots[0] > 0) && (listRoots[1] > 0))
        {
            Vec3 point1;
            point1.x = linePoint.x + listRoots.at(0)*lineDirn.x;
            point1.y = linePoint.y + listRoots.at(0)*lineDirn.y;
            point1.z = linePoint.z + listRoots.at(0)*lineDirn.z;

            Vec3 point2;
            point2.x = linePoint.x + listRoots.at(1)*lineDirn.x;
            point2.y = linePoint.y + listRoots.at(1)*lineDirn.y;
            point2.z = linePoint.z + listRoots.at(1)*lineDirn.z;

            // save the point nearest to the ray's origin
            if(linePoint.DistanceTo(point1) > linePoint.DistanceTo(point2))
            {   nearXsecPoint = point2;   }
            else
            {   nearXsecPoint = point1;   }

            return true;
        }
    }
    return false;
}

bool MapRenderer::calcCameraViewExtents(const Vec3 &camEye,
                                        const Vec3 &camViewpoint,
                                        const Vec3 &camUp,
                                        const double &camFovY,
                                        const double &camAspectRatio,
                                        double &camNearDist, double &camFarDist,
                                        double &camMinLat, double &camMaxLat,
                                        double &camMinLon, double &camMaxLon)
{
    Vec3 camAlongViewpoint = camViewpoint-camEye;

    // calculate four edge vectors of the frustum
    double camFovY_rad_bi = (camFovY*K_PI/180.0)/2;
    double dAlongViewpoint = cos(camFovY_rad_bi);
    double dAlongUp = sin(camFovY_rad_bi);
    double dAlongRight = dAlongUp*camAspectRatio;

    Vec3 ecefCenter;
    Vec3 camRight = camAlongViewpoint.Cross(camUp);
    Vec3 vAlongViewpoint = camAlongViewpoint.Normalized().ScaledBy(dAlongViewpoint);
    Vec3 vAlongUp = camUp.Normalized().ScaledBy(dAlongUp);
    Vec3 vAlongRight = camRight.Normalized().ScaledBy(dAlongRight);

    Vec3 viewTL = vAlongViewpoint + vAlongUp - vAlongRight;
    Vec3 viewTR = vAlongViewpoint + vAlongUp + vAlongRight;
    Vec3 viewBL = vAlongViewpoint - vAlongUp - vAlongRight;
    Vec3 viewBR = vAlongViewpoint - vAlongUp + vAlongRight;

    std::vector<Vec3> listFrustumEdgeVectors(4);
    listFrustumEdgeVectors[0] = viewTL;
    listFrustumEdgeVectors[1] = viewTR;
    listFrustumEdgeVectors[2] = viewBL;
    listFrustumEdgeVectors[3] = viewBR;

    // determine the camera parameters based on which
    // frustum edge vectors intersect with the Earth
    std::vector<bool> listIntersectsEarth(4);
    std::vector<Vec3> listIntersectionPoints(4);
    bool allIntersect = true;   // indicates all camera vectors intersect Earth's surface
    bool noneIntersect = true;  // indicates no camera vectors intersect Earth's surface

    for(int i=0; i < listFrustumEdgeVectors.size(); i++)
    {
        listIntersectsEarth[i] =
                calcRayEarthIntersection(camEye,
                                         listFrustumEdgeVectors[i],
                                         listIntersectionPoints[i]);

        if(!listIntersectsEarth[i])
        {
            // if any frustum vectors do not intersect Earth's surface,
            // intersect the vectors with a plane through Earth's
            // center that is normal to the camera's view direction

            // (the corresponding POI represents the horizon at some
            // arbitrary height -- we ignore altitude data anyway)

            bool intersectsPlane =
                calcRayPlaneIntersection(camEye, listFrustumEdgeVectors[i],
                                         ecefCenter, camAlongViewpoint,
                                         listIntersectionPoints[i]);

            // if the any of the camera vectors do not intersect the
            // center plane, assume the camera is invalid
            if(!intersectsPlane)
            {   return false;   }
        }

        allIntersect = allIntersect && listIntersectsEarth[i];
        noneIntersect = noneIntersect && !listIntersectsEarth[i];

//        OSRDEBUG << "INFO: POI (" << listIntersectionPoints[i].x
//                 << "  " << listIntersectionPoints[i].y
//                 << "  " << listIntersectionPoints[i].z << ")";
    }

    if(allIntersect)
    {
        // set the near clipping plane distance to be
        // 1/3 the distance between camEye and the Earth
        Vec3 earthSurfacePoint;
        calcRayEarthIntersection(camEye,
                                 camAlongViewpoint,
                                 earthSurfacePoint);

        camNearDist = camEye.DistanceTo(earthSurfacePoint)*(3.0/4.0);
    }
    else
    {
        // set near clipping plane to 1/3 of the minimum
        // distance between camEye and view frustum
        // intersection points (with Earth or center plane)
        double minDist = 0;
        std::vector<double> listIntersectionDist2s(4);
        for(int i=0; i < listFrustumEdgeVectors.size(); i++)
        {
            listIntersectionDist2s[i] =
                    camEye.Distance2To(listIntersectionPoints[i]);

            if(i == 0)
            {   minDist = listIntersectionDist2s[i];   }
            else
            {
                if(listIntersectionDist2s[i] < minDist)
                {   minDist = listIntersectionDist2s[i];   }
            }
        }

        camNearDist = sqrt(minDist)*(3.0/4.0);
    }

    // set the far clipping plane to be the distance
    // between the camera eye and a plane through Earth's
    // center that is normal to the camera's view direction
    // (represents distance between camEye and horizon)
    camFarDist = calcMinPointPlaneDistance(camEye,ecefCenter,
                                           camAlongViewpoint);

    // find and save view extents in LLA
    PointLLA pointLLA1,pointLLA2;
    convECEFToLLA(listIntersectionPoints[0],pointLLA1);
    convECEFToLLA(listIntersectionPoints[3],pointLLA2);

    if(pointLLA1.lat < pointLLA2.lat)
    {   camMinLat = pointLLA1.lat;   camMaxLat = pointLLA2.lat;   }
    else
    {   camMinLat = pointLLA2.lat;   camMaxLat = pointLLA1.lat;   }

    if(pointLLA1.lon < pointLLA2.lon)
    {   camMinLon = pointLLA1.lon;   camMaxLon = pointLLA2.lon;   }
    else
    {   camMinLon = pointLLA2.lon;   camMaxLon = pointLLA2.lon;   }

    return true;
}

bool MapRenderer::buildEarthSurfaceGeometry(unsigned int latSegments,
                                            unsigned int lonSegments,
                                            std::vector<Vec3> &myVertices,
                                            std::vector<Vec3> &myNormals,
                                            std::vector<Vec2> &myTexCoords,
                                            std::vector<unsigned int> &myIndices)
{
    Vec3 pointECEF;
    double latStepSize = 180.0f / double(latSegments);
    double lonStepSize = 360.0f / double(lonSegments);

    if(latStepSize < 4 || lonStepSize < 4)
    {
        OSRDEBUG << "ERROR: Insufficient lat/lon segments for Earth geometry";
        return false;
    }

    for(int i=0; i <= latSegments; i++)
    {
        for(int j=0; j <= lonSegments; j++)
        {
            double myLat = 90.0 - (i*latStepSize);
            double myLon = -180.0 + (j*lonStepSize);

            convLLAToECEF(PointLLA(myLat,myLon),pointECEF);
            myVertices.push_back(pointECEF);
            myNormals.push_back(myVertices.back());
            myTexCoords.push_back(Vec2(((myLon+180.0)/360.0),
                                          (myLat+90.0)/180.0));
        }
    }

    for(int i=0; i < latSegments; i++)
    {
        int pOffset = (lonSegments+1)*i;

        if(i != latSegments-1)
        {
            for(int j=pOffset; j < pOffset+lonSegments+1; j++)
            {
                myIndices.push_back(j);
                myIndices.push_back(j+lonSegments);
                myIndices.push_back(j+lonSegments+1);

                if(i > 0)
                {
                    myIndices.push_back(j);
                    myIndices.push_back(j+lonSegments+1);
                    myIndices.push_back(j+1);
                }
            }
        }
        else
        {
            for(int j=pOffset; j < pOffset+lonSegments+1; j++)
            {
                myIndices.push_back(j+lonSegments+1);
                myIndices.push_back(j+1);
                myIndices.push_back(j);
            }
        }
    }

    return true;
}

void MapRenderer::getFontList(std::vector<std::string> &listFonts)
{
    std::vector<std::string> listFontsInStyle;
    std::vector<std::string>::iterator it;

    for(int i=0; i < m_listRenderStyleConfigs.size(); i++)
    {
        m_listRenderStyleConfigs[i]->GetFontList(listFontsInStyle);

        for(int j=0; j < listFontsInStyle.size(); j++)
        {   listFonts.push_back(listFontsInStyle[j]);   }
    }

    std::sort(listFonts.begin(),listFonts.end());
    it = std::unique(listFonts.begin(),listFonts.end());

    listFonts.resize(it-listFonts.begin());
}


}

