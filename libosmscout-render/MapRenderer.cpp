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
{}

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
    // current scene data, so we clear all scene data

    //RemoveAllObjectsFromScene();

    m_listWayDataLists.clear();
    m_listWayDataLists.resize(listStyleConfigs.size());
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
    {   OSRDEBUG << "WARN: Could not calculate view extents";   }
    else
    {   updateSceneBasedOnCamera();   }
}

void MapRenderer::PanCamera(const Vec3 &dirnVec, double distMeters)
{
    Vec3 moveVec = (dirnVec.Normalized().ScaledBy(distMeters));
    m_camera.eye = m_camera.eye+moveVec;
    m_camera.viewPt = m_camera.viewPt+moveVec;

    if(!calcCameraViewExtents())
    {   OSRDEBUG << "WARN: Could not calculate view extents";   }
    else
    {   updateSceneBasedOnCamera();   }
}

void MapRenderer::ZoomCamera(double zoomAmount)
{
    Vec3 viewDirn = (m_camera.viewPt-m_camera.eye).Normalized();
    m_camera.eye = m_camera.eye + viewDirn.ScaledBy(zoomAmount);

    if(!calcCameraViewExtents())
    {   OSRDEBUG << "WARN: Could not calculate view extents";   }
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

    std::vector<std::vector<NodeRef> >      listNodeRefLists(listLODRanges.size());
    std::vector<std::vector<WayRef> >       listWayRefLists(listLODRanges.size());
    std::vector<std::vector<WayRef> >       listAreaRefLists(listLODRanges.size());
    std::vector<std::vector<RelationRef> >  listRelationWayLists(listLODRanges.size());
    std::vector<std::vector<RelationRef> >  listRelationAreaLists(listLODRanges.size());

    for(int i=0; i < listLODRanges.size(); i++)
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
            m_listRenderStyleConfigs.at(i)->GetWayTypesByPrio(listTypeIds);

            if(m_database->GetObjects(queryMinLon,queryMinLat,
                                      queryMaxLon,queryMaxLat,
                                      listTypeIds,
                                      listNodeRefLists[i],
                                      listWayRefLists[i],
                                      listAreaRefLists[i],
                                      listRelationWayLists[i],
                                      listRelationAreaLists[i]))
            {
                // the object ref lists need to be sorted according to id
                std::sort(listWayRefLists[i].begin(),
                          listWayRefLists[i].end(),
                          CompareWayRefs);

                // we retrieve objects from a high LOD (close up zoom)
                // to a lower LOD (far away zoom)

                // since the database query does not have finite resolution,
                // we cull all results that have already been retrieved for
                // all previous LOD ranges to prevent duplicates

                unsigned int numDupes = 0;
                for(int j=0; j < i; j++)
                {
                    std::vector<WayRef>::iterator it;
                    for(it = listWayRefLists[i].begin();
                        it != listWayRefLists[i].end();)
                    {
                        // std::lower_bound will return the first
                        // element that is NOT less than *it (the
                        // element can be equal to *it)
                        std::vector<WayRef>::iterator itDupe =
                                std::lower_bound(listWayRefLists[j].begin(),
                                                 listWayRefLists[j].end(),
                                                 *it,CompareWayRefs);

                        // if the iterator returned by lower_bound
                        // is equal to *it, there's a duplicate, so
                        // erase that element from the current range
                        if(itDupe != listWayRefLists[j].end() &&
                                (*itDupe)->GetId() == (*it)->GetId())
                        {
                            it = listWayRefLists[i].erase(it);
                            numDupes++;
                        }
                        else
                        {   ++it;   }
                    }
                }
            }
        }
    }

    // for the set of queried object refs in each lod range
    for(int i=0; i < numLodRanges; i++)
    {
//        OSRDEBUG << "INFO: Updating Scene Contents for LOD Range " << i;
//        OSRDEBUG << "INFO: listWayRefLists[" << i << "]size: " << listWayRefLists[i].size();
//        OSRDEBUG << "INFO: listWayDataLists[" << i << "]size: " << m_listWayDataLists[i].size();

        updateWayRenderData(listWayRefLists[i],i);

//        OSRDEBUG << "INFO: > New listWayDataLists[" << i << "]size: " << m_listWayDataLists[i].size();
    }

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

    if(oldOverlap < 0.5 || newOverlap < 0.5)
    {   OSRDEBUG << "INFO: Updated Scene Contents!";   updateSceneContents();   }
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

void MapRenderer::updateWayRenderData(const std::vector<WayRef> &listWayRefs,
                                      int lodIdx)
{
    // if the new view extents have no objects,
    // clear the old view extents at this lod
    int i=lodIdx;
    double objectsRemoved = 0;
    if(listWayRefs.empty())
    {
        if(!m_listWayDataLists[i].empty())
        {
            objectsRemoved = m_listWayDataLists[i].size();
            //RemoveWaysInLodFromScene(i);
            m_listWayDataLists[i].clear();
        }
        OSRDEBUG << "INFO: > New listWayDataLists[" << i << "]size: 0";
        return;
    }

    // remove objects from the old view extents
    // not present in the new view extents
    std::set<WayRenderData>::iterator itOld;
    for(itOld = m_listWayDataLists[i].begin();
        itOld != m_listWayDataLists[i].end();)
    {
        std::vector<WayRef>::const_iterator itNew =
                std::lower_bound(listWayRefs.begin(),
                                 listWayRefs.end(),
                                 (*itOld).wayRef,
                                 CompareWayRefs);

        if(itNew != listWayRefs.end() &&
                (*itNew)->GetId() == (*itOld).wayRef->GetId())
        {
            // way exists in new view
            ++itOld;
        }
        else
        {   // way dne in new view -- remove it
            std::set<WayRenderData>::iterator itDelete = itOld;
            removeWayFromScene(*itDelete); ++itOld;
            m_listWayDataLists[i].erase(itDelete);
            objectsRemoved++;
        }
    }

    // add objects from the new view extents
    // not present in the old view extents
    double objectsAdded = 0;
    std::vector<WayRef>::const_iterator itNew;

    for(itNew = listWayRefs.begin();
        itNew != listWayRefs.end();
        ++itNew)
    {
        WayRenderData wayRenderData;
        wayRenderData.wayRef = (*itNew);

        itOld = m_listWayDataLists[i].find(wayRenderData);
        if(itOld == m_listWayDataLists[i].end())
        {   // way dne in old view -- add it
            genWayRenderData((*itNew),m_listRenderStyleConfigs[i],
                             wayRenderData);

            addWayToScene(wayRenderData);

            std::pair<std::set<WayRenderData>::iterator,bool> inResult;
            inResult = m_listWayDataLists[i].insert(wayRenderData);

            objectsAdded++;
        }
    }

    OSRDEBUG << "INFO: > Removed " << objectsRemoved
             << " from listWayDatalists[" << i << "]";

    OSRDEBUG << "INFO: > Added " << objectsAdded
             << " to listWayDatalists[" << i << "]";
}


// ========================================================================== //
// ========================================================================== //

void MapRenderer::genWayRenderData(const WayRef &wayRef,
                                   const RenderStyleConfig *renderStyle,
                                   WayRenderData &wayRenderData)
{
    // get way type
    TypeId wayType = wayRef->GetType();

    // set general way properties
    wayRenderData.wayRef = wayRef;
    wayRenderData.wayPrio = renderStyle->GetWayPrio(wayType);
    wayRenderData.lineRenderStyle = renderStyle->GetWayLineRenderStyle(wayType);
    wayRenderData.nameLabelRenderStyle = renderStyle->GetWayNameLabelRenderStyle(wayType);

    // build way geometry
    wayRenderData.listPointData.resize(wayRef->nodes.size());
    for(int i=0; i < wayRef->nodes.size(); i++)
    {
        wayRenderData.listPointData[i] =
                convLLAToECEF(PointLLA(wayRef->nodes[i].lat,
                                       wayRef->nodes[i].lon));
    }
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

bool MapRenderer::calcLinePlaneIntersection(const Vec3 &linePoint,
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

    if(uDenmr == 0)         // line is || to plane
    {   return false;   }

    intersectionPoint = linePoint + lineDirn.ScaledBy(uNumr/uDenmr);
    return true;
}

bool MapRenderer::calcLineEarthIntersection(const Vec3 &linePoint,
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
    {
        Vec3 point1;
        point1.x = linePoint.x + listRoots.at(0)*lineDirn.x;
        point1.y = linePoint.y + listRoots.at(0)*lineDirn.y;
        point1.z = linePoint.z + listRoots.at(0)*lineDirn.z;
//        std::cout << "POI1: (" << point1.x
//                  << "," << point1.y
//                  << "," << point1.z << ")" << std::endl;

        Vec3 point2;
        point2.x = linePoint.x + listRoots.at(1)*lineDirn.x;
        point2.y = linePoint.y + listRoots.at(1)*lineDirn.y;
        point2.z = linePoint.z + listRoots.at(1)*lineDirn.z;
//        std::cout << "POI2: (" << point2.x
//                  << "," << point2.y
//                  << "," << point2.z << ")" << std::endl;

        // save the point nearest to the ray's origin
        if(linePoint.DistanceTo(point1) > linePoint.DistanceTo(point2))
        {   nearXsecPoint = point2;   }
        else
        {   nearXsecPoint = point1;   }

        return true;
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
                calcLineEarthIntersection(camEye,
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
                calcLinePlaneIntersection(camEye, listFrustumEdgeVectors[i],
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
        calcLineEarthIntersection(camEye,
                                 camAlongViewpoint,
                                 earthSurfacePoint);

        camNearDist = camEye.DistanceTo(earthSurfacePoint)/3;
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

        camNearDist = sqrt(minDist)/3;
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


}

