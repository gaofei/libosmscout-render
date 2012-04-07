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

#include "RenderStyleConfigReader.h"

namespace osmscout
{

RenderStyleConfigReader::RenderStyleConfigReader(std::string const &filePath,
                                                 TypeConfig * typeConfig,
                                                 std::vector<RenderStyleConfig*> &listStyleConfigs)
{
    // load style desc json file
    json_t * jsonRoot = json_load_file(filePath.c_str(), 0, &m_jsonError);

    if(!jsonRoot)
    {   logJsonError();  return;   }

    // STYLECONFIGS
    json_t * jsonStyleConfigs = json_object_get(jsonRoot,"STYLECONFIGS");
    if(json_array_size(jsonStyleConfigs) < 1)
    {   OSRDEBUG << "No STYLECONFIG objects found";   return;   }

    for(int i=0; i < json_array_size(jsonStyleConfigs); i++)
    {
        // save new RenderStyleConfig
        RenderStyleConfig * myStyleConfig;
        myStyleConfig = new RenderStyleConfig(typeConfig);
        listStyleConfigs.push_back(myStyleConfig);

        json_t * jsonStyleConfig = json_array_get(jsonStyleConfigs,i);

        // minMag and maxMag
        double minDistance, maxDistance;
        json_t * jsonMinMag = json_object_get(jsonStyleConfig,"minDistance");
        json_t * jsonMaxMag = json_object_get(jsonStyleConfig,"maxDistance");
        if(!getMagRange(jsonMinMag,jsonMaxMag,minDistance,maxDistance))
        {   return;   }

        myStyleConfig->SetMinDistance(minDistance);
        myStyleConfig->SetMaxDistance(maxDistance);

        // TODO we shouldnt fail to read the style file if
        //      AREAS,WAYS, etc arent found (maybe throw
        //      up a warning though)

        // AREAS
        json_t * jsonListAreas = json_object_get(jsonStyleConfig,"AREAS");
        if(json_array_size(jsonListAreas) < 1)
        {   OSRDEBUG << "(WARN) No AREA objects found in range " << minDistance << "-" << maxDistance;   }

        for(int j=0; j < json_array_size(jsonListAreas); j++)
        {
            json_t * jsonArea = json_array_get(jsonListAreas,j);

            // TYPE
            json_t * jsonAreaType = json_object_get(jsonArea,"type");
            if(json_string_value(jsonAreaType) == NULL)
            {   OSRDEBUG << "Missing Area type";   return;   }

            TypeId areaType;
            std::string strTypeId(json_string_value(jsonAreaType));
            areaType = typeConfig->GetAreaTypeId(strTypeId);
            if(areaType == typeIgnore)
            {   OSRDEBUG << "Invalid Area type";   return;   }

            // FILL
            json_t * jsonFillRenderStyle = json_object_get(jsonArea,"FillStyle");
            FillRenderStyle areaFillRenderStyle;
            if(!getFillRenderStyle(jsonFillRenderStyle,areaFillRenderStyle))
            {   return;   }

            myStyleConfig->SetAreaFillRenderStyle(areaType,areaFillRenderStyle);


            // NAMELABELSTYLE (optional)
            json_t * jsonLabelRenderStyle = json_object_get(jsonArea,"NameLabelStyle");
            LabelRenderStyle areaNameLabelRenderStyle;
            if(!(jsonLabelRenderStyle == NULL))
            {
                if(!getLabelRenderStyle(jsonLabelRenderStyle,areaNameLabelRenderStyle))
                {   return;   }

                myStyleConfig->SetAreaNameLabelRenderStyle(areaType,areaNameLabelRenderStyle);
            }
        }


        // WAYS
        json_t * jsonListWays = json_object_get(jsonStyleConfig,"WAYS");
        if(json_array_size(jsonListWays) < 1)
        {   OSRDEBUG << "No WAY objects found";   return;   }

        for(int j=0; j < json_array_size(jsonListWays); j++)
        {
            json_t * jsonWay = json_array_get(jsonListWays,j);

            // TYPE
            json_t * jsonWayType = json_object_get(jsonWay,"type");
            if(json_string_value(jsonWayType) == NULL)
            {   OSRDEBUG << "Invalid Way type";   return;   }

            TypeId wayType;
            std::string strTypeId(json_string_value(jsonWayType));
            wayType = typeConfig->GetWayTypeId(strTypeId);
            if(wayType == typeIgnore)
            {   OSRDEBUG << "Unknown Way type";   return;   }


            // LAYER
            json_t * jsonWayPrio = json_object_get(jsonWay,"layer");
            if(jsonWayPrio == NULL)
            {   OSRDEBUG << "No layer found";   return;   }
            int wayPrio = json_number_value(jsonWayPrio);

            myStyleConfig->SetWayLayer(wayType,wayPrio);


            // LINESTYLE
            json_t * jsonLineRenderStyle = json_object_get(jsonWay,"LineStyle");
            LineRenderStyle wayLineRenderStyle;
            if(!getLineRenderStyle(jsonLineRenderStyle,wayLineRenderStyle))
            {   return;   }

            myStyleConfig->SetWayLineRenderStyle(wayType,wayLineRenderStyle);


            // NAMELABELSTYLE (optional)
            json_t * jsonLabelRenderStyle = json_object_get(jsonWay,"NameLabelStyle");
            LabelRenderStyle wayNameLabelRenderStyle;
            if(!(jsonLabelRenderStyle == NULL))
            {
                if(!getLabelRenderStyle(jsonLabelRenderStyle,wayNameLabelRenderStyle))
                {   return;   }

                myStyleConfig->SetWayNameLabelRenderStyle(wayType,wayNameLabelRenderStyle);
            }


            // REFLABELSTYLE (optional)
            // TODO
        }

        myStyleConfig->PostProcess();
    }
}

bool RenderStyleConfigReader::HasErrors()
{   return (m_listMessages.size() > 0);   }

void RenderStyleConfigReader::GetDebugLog(std::vector<std::string> &listDebugMessages)
{
    for(int i=0; i < m_listMessages.size(); i++)
    {   listDebugMessages.push_back(m_listMessages.at(i));   }
}

bool RenderStyleConfigReader::getMagRange(json_t *jsonMinMag, json_t *jsonMaxMag,
                                          double &minMag, double &maxMag)
{
    double minMagValue = json_number_value(jsonMinMag);
    double maxMagValue = json_number_value(jsonMaxMag);

    if(maxMagValue <= minMagValue)
    {   OSRDEBUG << "Invalid magnification range";   return false;   }
    else
    {
        minMag = minMagValue;
        maxMag = maxMagValue;
        return true;
    }
}

bool RenderStyleConfigReader::getFillRenderStyle(json_t *jsonFillStyle,
                                                 FillRenderStyle &fillRenderStyle)
{
    if(jsonFillStyle == NULL)
    {   OSRDEBUG << "FillStyle doesn't exist";   return false;   }

    // FillStyle.fillColor
    json_t * jsonFillColor = json_object_get(jsonFillStyle,"fillColor");
    if(json_string_value(jsonFillColor) == NULL)
    {   OSRDEBUG << "Invalid fillColor value";   return false;   }

    ColorRGBA fillColor;
    std::string strFillColor(json_string_value(jsonFillColor));
    if(!parseColorRGBA(strFillColor,fillColor))
    {   OSRDEBUG << "Could not parse fillColor string";   return false;   }


    // FillStyle.outlineColor
    json_t * jsonOutlineColor = json_object_get(jsonFillStyle,"outlineColor");
    if(json_string_value(jsonOutlineColor) == NULL)
    {   OSRDEBUG << "Invalid outlineColor value";   return false;   }

    ColorRGBA outlineColor;
    std::string strOutlineColor(json_string_value(jsonOutlineColor));
    if(!parseColorRGBA(strOutlineColor,outlineColor))
    {   OSRDEBUG << "Could not parse outlineColor string";   return false;   }

    json_t * jsonOutlineWidth = json_object_get(jsonFillStyle,"outlineWidth");
    double outlineWidth = json_number_value(jsonOutlineWidth);

    // save
    fillRenderStyle.SetFillColor(fillColor);
    fillRenderStyle.SetOutlineColor(outlineColor);
    fillRenderStyle.SetOutlineWidth(outlineWidth);

    return true;
}

bool RenderStyleConfigReader::getLineRenderStyle(json_t *jsonLineStyle,
                                                 LineRenderStyle &lineRenderStyle)
{
    if(jsonLineStyle == NULL)
    {   OSRDEBUG << "LineStyle doesn't exist";   return false;   }

    // LineStyle.lineWidth
    json_t * jsonLineWidth = json_object_get(jsonLineStyle,"lineWidth");
    double lineWidth = json_number_value(jsonLineWidth);

    // LineStyle.outlineWidth
    json_t * jsonOutlineWidth = json_object_get(jsonLineStyle,"outlineWidth");
    double outlineWidth = json_number_value(jsonOutlineWidth);

    // LineStyle.lineColor
    json_t * jsonLineColor = json_object_get(jsonLineStyle,"lineColor");
    if(json_string_value(jsonLineColor) == NULL)
    {   OSRDEBUG << "Invalid lineColor value";   return false;   }

    ColorRGBA lineColor;
    std::string strLineColor(json_string_value(jsonLineColor));
    if(!parseColorRGBA(strLineColor,lineColor))
    {   OSRDEBUG << "Could not parse lineColor string";   return false;   }

    // LineStyle.outlineColor
    json_t * jsonOutlineColor = json_object_get(jsonLineStyle,"outlineColor");
    if(json_string_value(jsonOutlineColor) == NULL)
    {   OSRDEBUG << "Invalid outlineColor value";   return false;   }

    ColorRGBA outlineColor;
    std::string strOutlineColor(json_string_value(jsonOutlineColor));
    if(!parseColorRGBA(strOutlineColor,outlineColor))
    {   OSRDEBUG << "Could not parse outlineColor string";   return false;   }

    // save
    lineRenderStyle.SetLineWidth(lineWidth);
    lineRenderStyle.SetOutlineWidth(outlineWidth);
    lineRenderStyle.SetLineColor(lineColor);
    lineRenderStyle.SetOutlineColor(outlineColor);

    return true;
}

bool RenderStyleConfigReader::getLabelRenderStyle(json_t *jsonLabelStyle,
                                                  LabelRenderStyle &labelRenderStyle)
{
    // LabelStyle.type (optional)
    LabelRenderStyleType labelType;
    json_t * jsonLabelType = json_object_get(jsonLabelStyle,"type");
    if(json_string_value(jsonLabelType) == NULL)
    {   labelType = LABEL_DEFAULT;   }
    else
    {
        std::string labelTypeStr(json_string_value(jsonLabelType));

        if(labelTypeStr.compare("contour") == 0)
        {   labelType = LABEL_CONTOUR;   }

        else if(labelTypeStr.compare("default") == 0)
        {   labelType = LABEL_DEFAULT;   }
    }

    // LabelStyle.fontFamily
    json_t * jsonFontFamily = json_object_get(jsonLabelStyle,"fontFamily");
    if(json_string_value(jsonFontFamily) == NULL)
    {   OSRDEBUG << "Invalid fontFamily";   return false;   }

    std::string fontFamily(json_string_value(jsonFontFamily));

    // LabelStyle.fontColor
    json_t * jsonFontColor = json_object_get(jsonLabelStyle,"fontColor");
    if(json_string_value(jsonFontColor) == NULL)
    {   OSRDEBUG << "Invalid fontColor";   return false;   }

    ColorRGBA fontColor;
    std::string strFontColor(json_string_value(jsonFontColor));
    if(!parseColorRGBA(strFontColor,fontColor))
    {   OSRDEBUG << "Invalid fontColor";   return false;   }

    // LabelStyle.fontSize
    json_t * jsonFontSize = json_object_get(jsonLabelStyle,"fontSize");
    double fontSize = json_number_value(jsonFontSize);
    if(fontSize < 0.0)
    {   OSRDEBUG << "Invalid fontSize (" << fontSize << ")";  return false;   }

    // LabelStyle.labelPadding
    if(labelType == LABEL_CONTOUR)
    {
        json_t * jsonLabelPadding = json_object_get(jsonLabelStyle,"padding");
        double labelPadding = json_number_value(jsonLabelPadding);

        if(labelPadding < 0.0)
        {   OSRDEBUG << "Invalid labelPadding (" << labelPadding << ")";   return false;   }

        labelRenderStyle.SetLabelPadding(labelPadding);
    }

    // save
    labelRenderStyle.SetFontFamily(fontFamily);
    labelRenderStyle.SetFontColor(fontColor);
    labelRenderStyle.SetFontSize(fontSize);
    labelRenderStyle.SetLabelType(labelType);

    return true;
}

bool RenderStyleConfigReader::parseColorRGBA(std::string const &strColor,
                                             ColorRGBA &myColor)
{
    // expect "#RRGGBBAA"
    if(strColor.length() != 9)
    {   return false;   }

    std::string myStrColor = strColor;
    myStrColor.erase(0,1);

    for(int i=0; i < 4; i++)
    {   // 0,1,2,3
        std::string strColorByte;
        strColorByte.append(1,myStrColor.at(i*2));
        strColorByte.append(1,myStrColor.at((i*2)+1));

        std::stringstream strStream;
        int fByte;

        strStream << std::hex << strColorByte;
        strStream >> fByte;

        switch(i)
        {
        case 0:
            myColor.R = (fByte/255.0); break;
        case 1:
            myColor.G = (fByte/255.0); break;
        case 2:
            myColor.B = (fByte/255.0); break;
        case 3:
            myColor.A = (fByte/255.0); break;
        default:
            break;
        }
    }

    return true;
}

void RenderStyleConfigReader::logJsonError()
{
    std::string errorText(m_jsonError.text);
    std::string errorLine(convIntToString(m_jsonError.line));
    std::string errorPos(convIntToString(m_jsonError.position));
    OSRDEBUG << errorText;
    OSRDEBUG << errorLine;
    OSRDEBUG << errorPos;
}

std::string RenderStyleConfigReader::convIntToString(int myInt)
{
    std::stringstream ss;
    std::string str;
    ss << myInt;
    ss >> str;
    return str;
}

}
