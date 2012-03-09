#ifndef OSMSCOUT_MAP_RENDERER_H
#define OSMSCOUT_MAP_RENDERER_H

// sys includes
#include <math.h>
#include <vector>

// osmscout includes
#include <osmscout/Database.h>
#include <osmscout/ObjectRef.h>
#include <osmscout/Way.h>

// osmscout-render includes
#include "Vec3.hpp"
#include "SimpleLogger.hpp"
#include "RenderStyleConfig.hpp"


// PI!
#define K_PI 3.141592653589

// WGS84 ellipsoid parameters
// (http://en.wikipedia.org/wiki/WGS_84)
#define ELL_SEMI_MAJOR 6378137.0            // meters
#define ELL_SEMI_MAJOR_EXP2 40680631590769

#define ELL_SEMI_MINOR 6356752.3142         // meters
#define ELL_SEMI_MINOR_EXP2 40408299984087.1

#define ELL_F 1/298.257223563
#define ELL_ECC_EXP2 6.69437999014e-3
#define ELL_ECC2_EXP2 6.73949674228e-3


namespace osmscout
{

// longitude, latitude, altitude point class
class PointLLA
{
public:
    PointLLA() :
        lon(0),lat(0),alt(0) {}

    PointLLA(double myLat, double myLon) :
        lat(myLat),lon(myLon) {}

    PointLLA(double myLat, double myLon, double myAlt) :
        lon(myLon),lat(myLat),alt(myAlt) {}

    double lon;
    double lat;
    double alt;
};

class WayRenderData
{
public:
    ObjectRef m_wayRef;
    LineRenderStyle const * m_lineRenderStyle;
};

class MapRenderer
{
public:
    MapRenderer(std::string const &dataPath);
    virtual ~MapRenderer();

    // SetRenderStyleConfig
    void SetRenderStyleConfig(std::vector<RenderStyleConfig*> const &listStyleConfigs);

    // GetDebugLog
    void GetDebugLog(std::vector<std::string> &listDebugMessages);

    // UpdateSceneContents
    // * this method takes the active camera's position and view
    //   projection (onto Earth's surface) to determine the map
    //   data that should be displayed, and calls the renderer
    //   driver's functions to update the scene
    // * call this function whenever the scene's view
    //   changes through panning/zooming/rotation (NOT
    //   every frame, but at the END of a view change)
    void UpdateSceneContents(Vec3 const &camEye,
                             double const &minLat,
                             double const &maxLat,
                             double const &minLon,
                             double const &maxLon);


    // convLLAToECEF
    // * converts point data in Latitude/Longitude/Altitude to
    //   its corresponding X/Y/Z in ECEF coordinates
    void convLLAToECEF(PointLLA const &pointLLA, Vec3 &pointECEF);

    // convECEFToLLA
    // * converts point data in ECEF X/Y/Z to its corresponding
    //   Longitude/Latitude/Altitude coordinates
    void convECEFToLLA(Vec3 const &pointECEF, PointLLA &pointLLA);

    // convWayPathToOffsets
    // * offsets ordered way point data to give the line 'thickness'
    //   as an actual 2D polygon representing a road/street
    void convWayPathToOffsets(std::vector<Vec3> const &listWayPoints,
                              std::vector<Vec3> &listOffsetPointsA,
                              std::vector<Vec3> &listOffsetPointsB,
                              Vec3 &pointEarthCenter,
                              double lineWidth);

    // calcQuadraticEquationReal
    // * computes the solutions to a quadratic equation with
    //   parameters a, b and c, and accounts for numerical error
    //   note: doesn't work with complex roots (will save empty vector)
    void calcQuadraticEquationReal(double a, double b, double c,
                                   std::vector<double> &listRoots);

    // calcLinePlaneMinDistance
    // * computes the minimum distance between a given
    //   point and plane
    double calcMinPointPlaneDistance(Vec3 const &distalPoint,
                                     Vec3 const &planePoint,
                                     Vec3 const &planeNormal);

    // calcLinePlaneIntersection
    // * computes the intersection point between a given
    //   line and plane
    // * returns false if no intersection point exists
    bool calcLinePlaneIntersection(Vec3 const &linePoint,
                                   Vec3 const &lineDirn,
                                   Vec3 const &planePoint,
                                   Vec3 const &planeNormal,
                                   Vec3 &intersectionPoint);

    // calcRayEarthIntersection
    // * computes the nearest intersection point (to the ray's
    //   origin) with the surface of the Earth defined with
    //   ECEF coordinates
    // * return true if at least one intersection point found
    //   else return false
    bool calcLineEarthIntersection(Vec3 const &rayPoint,
                                  Vec3 const &rayDirn,
                                  Vec3 &nearXsecPoint);

    // calcCameraViewExtents
    // * uses the camera's view frustum to find the view
    //   extents of the camera, in terms of a lat/lon box
    // * return true if the Earth is visible with the
    //   camera's parameters, else return false (camNearDist,
    //   camFarDist, camView[]Corner are invalid in this case
    bool calcCameraViewExtents(Vec3 const &camEye,
                               Vec3 const &camViewpoint,
                               Vec3 const &camUp,
                               double const &camFovY,
                               double const &camAspectRatio,
                               double &camNearDist, double &camFarDist,
                               double &camMinLat, double &camMaxLat,
                               double &camMinLon, double &camMaxLon);





private:
    DatabaseParameter m_databaseParam;
    Database m_database;

    std::vector<std::string> m_listMessages;
    std::vector<osmscout::RenderStyleConfig*> m_listRenderStyleConfigs;
};

}

#endif
