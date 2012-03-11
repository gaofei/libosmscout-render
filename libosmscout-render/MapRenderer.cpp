#include "MapRenderer.h"


namespace osmscout
{

MapRenderer::MapRenderer(Database const *myDatabase) :
    m_database(myDatabase)
{}

MapRenderer::~MapRenderer()
{}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::SetRenderStyleConfigs(const std::vector<RenderStyleConfig*> &listStyleConfigs)
{
    m_listRenderStyleConfigs.clear();

    for(int i=0; i < listStyleConfigs.size(); i++)
    {   m_listRenderStyleConfigs.push_back(listStyleConfigs.at(i));   }
}

void MapRenderer::GetDebugLog(std::vector<std::string> &listDebugMessages)
{
    for(int i=0; i < m_listMessages.size(); i++)
    {   listDebugMessages.push_back(m_listMessages.at(i));   }
}

// ========================================================================== //
// ========================================================================== //

void MapRenderer::UpdateSceneContents(const Vec3 &camEye,
                                      const double &minLat,
                                      const double &maxLat,
                                      const double &minLon,
                                      const double &maxLon)
{

    // shouldnt we be calling calcCameraViewExtents in here??

    // establish which RenderStyleConfig is 'active'
    // given the current camEye and lat/lon bounds
    PointLLA pointCornerSW(minLat,minLon);
    PointLLA pointCornerNE(maxLat,maxLon);
    Vec3 ecefCornerSW;    convLLAToECEF(pointCornerSW,ecefCornerSW);
    Vec3 ecefCornerNE;    convLLAToECEF(pointCornerNE,ecefCornerNE);
    Vec3 ecefSectorCenter = (ecefCornerSW+ecefCornerNE).ScaledBy(0.5);
    double distCamSector = ecefSectorCenter.DistanceTo(camEye);
    OSRDEBUG << "INFO: Estimated ECEF View Center: ("
             << ecefSectorCenter.x << ","
             << ecefSectorCenter.y << ","
             << ecefSectorCenter.z << ")";

    OSRDEBUG << "INFO: Current Rough/Average Camera Distance: " << distCamSector;

    size_t r=0;
    bool foundStyle = false;
    for(r=0; r < m_listRenderStyleConfigs.size(); r++)
    {
        if(distCamSector >= m_listRenderStyleConfigs.at(r)->GetMinDistance() &&
           distCamSector < m_listRenderStyleConfigs.at(r)->GetMaxDistance())
        {
            OSRDEBUG << "INFO: Style Config " << r << " is valid between "
                     << m_listRenderStyleConfigs.at(r)->GetMinDistance()
                     << "," << m_listRenderStyleConfigs.at(r)->GetMaxDistance();
            foundStyle = true;
            break;
        }
    }

    if(!foundStyle)
    {
        OSRDEBUG << "WARN: UpdateSceneContents: No RenderStyleConfig "
                 << "found for camEye distance: " << distCamSector;
        return;
    }

    std::vector<TypeId> listTypeIds;
    m_listRenderStyleConfigs.at(r)->GetWayTypesByPrio(listTypeIds);

    // get objects from database
    std::vector<NodeRef>        listNodeRefs;
    std::vector<WayRef>         listWayRefs;
    std::vector<WayRef>         listAreaRefs;
    std::vector<RelationRef>    listRelationWays;
    std::vector<RelationRef>    listRelationAreas;

    if(m_database->GetObjects(minLon,minLat,
                              maxLon,maxLat,
                              listTypeIds,
                              listNodeRefs,
                              listWayRefs,
                              listAreaRefs,
                              listRelationWays,
                              listRelationAreas));

    OSRDEBUG << "INFO: Database Query Result: ";
    OSRDEBUG << "INFO: " << listTypeIds.size() << " TypeIds";
    OSRDEBUG << "INFO: " << listNodeRefs.size() << " NodeRefs";
    OSRDEBUG << "INFO: " << listWayRefs.size() << " WayRefs";
    OSRDEBUG << "INFO: " << listAreaRefs.size() << " AreaRefs";
    OSRDEBUG << "INFO: " << listRelationWays.size() << " RelationWays";
    OSRDEBUG << "INFO: " << listRelationAreas.size() << " RelationAreas";
}

void MapRenderer::UpdateSceneContents(const Vec3 &camEye,
                                      const Vec3 &camViewpoint,
                                      const Vec3 &camUp,
                                      const double &camFovY,
                                      const double &camAspectRatio,
                                      double &camNearDist,
                                      double &camFarDist)
{
    // compute the scene view extents
    double minLat,minLon,maxLat,maxLon;
    if(!calcCameraViewExtents(camEye,camViewpoint,camUp,
                              camFovY,camAspectRatio,
                              camNearDist,camFarDist,
                              minLat,maxLat,minLon,maxLon))
    {   OSRDEBUG << "WARN: Could not calculate view extents";   return;   }

    // calculate the minimum and maximum distance to
    // camEye within the available lat/lon bounds

    PointLLA camSurfaceLatLon;
    convECEFToLLA(camEye,camSurfaceLatLon);
    camSurfaceLatLon.alt = 0;

    std::vector<PointLLA> listLODBoundsFromCamSW;
    std::vector<PointLLA> listLODBoundsFromCamNE;


    PointLLA pointCornerSW(minLat,minLon);
    PointLLA pointCornerNE(maxLat,maxLon);

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

double MapRenderer::calcMinPointLineDistance(const double pointX, const double pointY,
                                             const double lineAX, const double lineAY,
                                             const double lineBX, const double lineBY)
{
    // ref: http://paulbourke.net/geometry/pointline/

    double uDenr = (lineAX-lineBX)*(lineAX-lineBX) + (lineAY-lineBY)*(lineAY-lineBY);

    if(uDenr == 0)
    {   // indicates that pointA and pointB are coincident
        return sqrt((pointX-lineAX)*(pointX-lineAX) +
                    (pointY-lineAY)*(pointY-lineAY));
    }

    double uNumr = (pointX-lineAX)*(lineBX-lineAX) + (pointY-lineAY)*(lineBY-lineAY);
    double u = uNumr/uDenr;

    if(u > 0 && u <= 1)
    {
        // indicates the projected point lies between
        // the given line segment endpoints
        double pX = lineAX + u*(lineBX-lineAX);
        double pY = lineAY + u*(lineBY-lineAY);
        return sqrt((pointX-pX)*(pointX-pX) +
                    (pointY-pY)*(pointY-pY));
    }
    else
    {
        // indicates the projected point lies outside
        // the given line segment endpoints
        double distA = sqrt((pointX-lineAX)*(pointX-lineAX) +
                            (pointY-lineAY)*(pointY-lineAY));

        double distB = sqrt((pointX-lineBX)*(pointX-lineBX) +
                            (pointY-lineBY)*(pointY-lineBY));

        return (distA < distB) ? distA : distB;
    }
}

double MapRenderer::calcMinPointRectDistance(const double pointX, const double pointY,
                                             const double rectBLX, const double rectBLY,
                                             const double rectTRX, const double rectTRY)
{
    // first check if the point is inside the rectangle,
    // and return zero if it is
    if(pointX >= rectBLX && pointX <= rectTRX)
    {
        if(pointY >= rectBLY && pointY <= rectTRY)
        {
            return 0;
        }
    }

    // find the minimum of the perpendicular distances between the
    // given point and each of the rectangle line segments
    double distLeft,distRight,distTop,distBottom;
    distLeft = calcMinPointLineDistance(pointX,pointY,rectBLX,rectBLY,rectBLX,rectTRY);
    distRight = calcMinPointLineDistance(pointX,pointY,rectTRX,rectTRY,rectTRX,rectBLY);
    distTop = calcMinPointLineDistance(pointX,pointY,rectBLX,rectTRY,rectTRX,rectTRY);
    distBottom = calcMinPointLineDistance(pointX,pointY,rectBLX,rectBLY,rectTRX,rectBLY);

    double minDist = distLeft;

    if(distRight < minDist)
    {   minDist = distRight;   }

    if(distTop < minDist)
    {   minDist = distTop;   }

    if(distBottom < minDist)
    {   minDist = distBottom;   }

    return minDist;
}

double MapRenderer::calcMaxPointRectDistance(const double pointX, const double pointY,
                                             const double rectBLX, const double rectBLY,
                                             const double rectTRX, const double rectTRY)
{
    // return the max distance between the point and all
    // four corners of the rectangle

    double distBL, distBR, distTL, distTR;
    distBL = sqrt((pointX-rectBLX)*(pointX-rectBLX)+(pointY-rectBLY)*(pointY-rectBLY));
    distBR = sqrt((pointX-rectTRX)*(pointX-rectTRX)+(pointY-rectBLY)*(pointY-rectBLY));
    distTL = sqrt((pointX-rectBLX)*(pointX-rectBLX)+(pointY-rectTRY)*(pointY-rectTRY));
    distTR = sqrt((pointX-rectTRX)*(pointX-rectTRX)+(pointY-rectTRY)*(pointY-rectTRY));

    double maxDist = distBL;

    if(distBR > maxDist)
    {   maxDist = distBR;   }

    if(distTL > maxDist)
    {   maxDist = distTL;   }

    if(distTR > maxDist)
    {   maxDist = distTR;   }

    return maxDist;
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

        OSRDEBUG << "POI (" << listIntersectionPoints[i].x
                 << "  " << listIntersectionPoints[i].y
                 << "  " << listIntersectionPoints[i].z << ")";
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

void MapRenderer::convWayPathToOffsets(const std::vector<Vec3> &listWayPoints,
                                       std::vector<Vec3> &listOffsetPointsA,
                                       std::vector<Vec3> &listOffsetPointsB,
                                       Vec3 &pointEarthCenter,
                                       double lineWidth)
{}









}

