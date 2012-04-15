#ifndef OSMSCOUT_RENDERSTYLECONFIG_HPP
#define OSMSCOUT_RENDERSTYLECONFIG_HPP

#include <set>
#include <string>
#include <limits>
#include <vector>
#include <algorithm>

#include <osmscout/Types.h>
#include <osmscout/TypeConfig.h>

namespace osmscout
{

    // ========================================================================== //
    // ========================================================================== //

    class OSMSCOUT_API ColorRGBA
    {
    public:
        ColorRGBA() : R(1), G(1), B(1), A(1)
        {}

        ColorRGBA& operator=(ColorRGBA const &otherColor)
        {
            this->R = otherColor.R;
            this->G = otherColor.G;
            this->B = otherColor.B;
            this->A = otherColor.A;
            return *this;
        }

        double R;
        double G;
        double B;
        double A;
    };

    // ========================================================================== //
    // ========================================================================== //

    enum SymbolRenderStyleType
    {
        // should I just billboard everything
        // and use 2d shapes? whats the adv
        // of using 3d shapes here at all?
        SYMBOL_TRIANGLE,
        SYMBOL_SQUARE,
        SYMBOL_CIRCLE
    };

    class SymbolRenderStyle
    {
    public:
        SymbolRenderStyle() :
            m_id(0),m_offsetHeight(0),m_symbolSize(0),
            m_symbolType(SYMBOL_SQUARE)
        {}

        SymbolRenderStyle(SymbolRenderStyle const &symbolRenderStyle)
        {
            m_id = symbolRenderStyle.GetId();
            m_offsetHeight = symbolRenderStyle.GetOffsetHeight();
            m_symbolSize = symbolRenderStyle.GetSymbolSize();
            m_symbolType = symbolRenderStyle.GetSymbolType();
        }

        void SetId(unsigned int symbolId)
        {   m_id = symbolId;   }

        void SetSymbolSize(double symbolSize)
        {   m_symbolSize = symbolSize;   }

        void SetOffsetHeight(double offsetHeight)
        {   m_offsetHeight = offsetHeight;   }

        void SetSymbolType(SymbolRenderStyleType symbolType)
        {   m_symbolType = symbolType;   }


        unsigned int GetId() const
        {   return m_id;   }

        double GetSymbolSize() const
        {   return m_symbolSize;   }

        double GetOffsetHeight() const
        {   return m_offsetHeight;   }

        SymbolRenderStyleType GetSymbolType() const
        {   return m_symbolType;   }

    private:
        unsigned int m_id;
        double m_offsetHeight;
        double m_symbolSize;
        SymbolRenderStyleType m_symbolType;
    };

    // ========================================================================== //
    // ========================================================================== //

    class OSMSCOUT_API FillRenderStyle
    {
    public:
        FillRenderStyle() :
            m_id(0),
            m_outlineWidth(0)
        {}

        FillRenderStyle(FillRenderStyle const &fillRenderStyle)
        {
            m_id = fillRenderStyle.GetId();
            m_fillColor = fillRenderStyle.GetFillColor();
            m_outlineColor = fillRenderStyle.GetOutlineColor();
            m_outlineWidth = fillRenderStyle.GetOutlineWidth();
        }

        void SetId(unsigned int fillId)
        {   m_id = fillId;   }

        void SetFillColor(ColorRGBA const &fillColor)
        {   m_fillColor = fillColor;   }

        void SetOutlineColor(ColorRGBA const &outlineColor)
        {   m_outlineColor = outlineColor;   }

        void SetOutlineWidth(double outlineWidth)
        {   m_outlineWidth = outlineWidth;   }

        inline unsigned int GetId() const
        {   return m_id;   }

        inline ColorRGBA GetFillColor() const
        {   return m_fillColor;   }

        inline ColorRGBA GetOutlineColor() const
        {   return m_outlineColor;   }

        inline double GetOutlineWidth() const
        {   return m_outlineWidth;   }

    private:
        unsigned int m_id;
        ColorRGBA m_fillColor;
        ColorRGBA m_outlineColor;
        double m_outlineWidth;
    };

    // ========================================================================== //
    // ========================================================================== //

    class OSMSCOUT_API LineRenderStyle
    {
    public:
        LineRenderStyle() :
            m_id(0),
            m_lineWidth(0),
            m_outlineWidth(0)
        {}

        LineRenderStyle(LineRenderStyle const &lineRenderStyle)
        {
            m_id = lineRenderStyle.GetId();
            m_lineWidth = lineRenderStyle.GetLineWidth();
            m_lineColor = lineRenderStyle.GetLineColor();
            m_outlineWidth = lineRenderStyle.GetOutlineWidth();
            m_outlineColor = lineRenderStyle.GetOutlineColor();
        }

        void SetId(unsigned int lineId)
        {   m_id = lineId;   }

        void SetLineWidth(double lineWidth)
        {   m_lineWidth = (lineWidth > 1) ? lineWidth : 1;   }

        void SetOutlineWidth(double outlineWidth)
        {   m_outlineWidth = outlineWidth;   }

        void SetLineColor(ColorRGBA const & lineColor)
        {   m_lineColor = lineColor;   }

        void SetOutlineColor(ColorRGBA const & outlineColor)
        {   m_outlineColor = outlineColor;   }

        inline unsigned int GetId() const
        {   return m_id;   }

        inline double GetLineWidth() const
        {   return m_lineWidth;   }

        inline double GetOutlineWidth() const
        {   return m_outlineWidth;   }

        inline ColorRGBA GetLineColor() const
        {   return m_lineColor;   }

        inline ColorRGBA GetOutlineColor() const
        {   return m_outlineColor;   }

    private:
        unsigned int m_id;
        double      m_lineWidth;
        double      m_outlineWidth;
        ColorRGBA   m_lineColor;
        ColorRGBA   m_outlineColor;
    };

    // ========================================================================== //
    // ========================================================================== //

    enum LabelRenderStyleType
    {
        LABEL_DEFAULT,
        LABEL_PLATE,
        LABEL_CONTOUR
    };

    class OSMSCOUT_API LabelRenderStyle
    {
    public:
        LabelRenderStyle() :
            m_id(0),
            m_fontSize(10.0),
            m_fontOutlineSize(10.0),
            m_contourPadding(0.5),
            m_offsetHeight(5.0),
            m_platePadding(1.0),
            m_plateOutlineWidth(0.2)
        {}

        LabelRenderStyle(LabelRenderStyle const &labelRenderStyle)
        {
            m_id = labelRenderStyle.GetId();
            m_fontSize = labelRenderStyle.GetFontSize();
            m_fontColor = labelRenderStyle.GetFontColor();
            m_fontFamily = labelRenderStyle.GetFontFamily();
            m_fontOutlineSize = labelRenderStyle.GetFontOutlineSize();
            m_fontOutlineColor = labelRenderStyle.GetFontOutlineColor();
            m_labelType = labelRenderStyle.GetLabelType();

            m_contourPadding = labelRenderStyle.GetContourPadding();

            m_offsetHeight = labelRenderStyle.GetOffsetHeight();

            m_platePadding = labelRenderStyle.GetPlatePadding();
            m_plateColor = labelRenderStyle.GetPlateColor();
            m_plateOutlineWidth = labelRenderStyle.GetPlateOutlineWidth();
            m_plateOutlineColor = labelRenderStyle.GetPlateOutlineColor();
        }

        // SET methods for all label types
        void SetId(unsigned int labelId)
        {   m_id = labelId;   }

        void SetFontSize(double fontSize)
        {   m_fontSize = fontSize;   }

        void SetFontColor(ColorRGBA const &fontColor)
        {   m_fontColor = fontColor;   }

        void SetFontFamily(const std::string &fontFamily)
        {   m_fontFamily = fontFamily;   }

        void SetFontOutlineSize(double fontOutlineSize)
        {   m_fontOutlineSize = fontOutlineSize;   }

        void SetFontOutlineColor(const ColorRGBA &fontOutlineColor)
        {   m_fontOutlineColor = fontOutlineColor;   }

        void SetLabelType(LabelRenderStyleType labelType)
        {   m_labelType = labelType;   }

        // SET methods for contour only
        void SetContourPadding(double contourPadding)
        {   m_contourPadding = contourPadding;  }

        // SET methods for default and plate
        void SetOffsetHeight(double offsetHeight)
        {   m_offsetHeight = offsetHeight;   }

        // SET methods for plate only
        void SetPlatePadding(double platePadding)
        {   m_platePadding = platePadding;   }

        void SetPlateColor(ColorRGBA const &plateColor)
        {   m_plateColor = plateColor;   }

        void SetPlateOutlineWidth(double plateOutlineWidth)
        {   m_plateOutlineWidth = plateOutlineWidth;   }

        void SetPlateOutlineColor(ColorRGBA const &plateOutlineColor)
        {   m_plateOutlineColor = plateOutlineColor;   }

        // GET methods for all label types
        inline unsigned int GetId() const
        {   return m_id;   }

        inline double GetFontSize() const
        {   return m_fontSize;   }

        inline ColorRGBA GetFontColor() const
        {   return m_fontColor;   }

        inline std::string GetFontFamily() const
        {   return m_fontFamily;   }

        inline double GetFontOutlineSize() const
        {   return m_fontOutlineSize;   }

        inline ColorRGBA GetFontOutlineColor() const
        {   return m_fontOutlineColor;   }

        inline LabelRenderStyleType GetLabelType() const
        {   return m_labelType;   }

        // GET methods for contour only
        inline double GetContourPadding() const
        {   return m_contourPadding;   }

        // GET methods for default and plate
        inline double GetOffsetHeight() const
        {   return m_offsetHeight;   }

        // GET methods for plate only
        inline double GetPlatePadding() const
        {   return m_platePadding;   }

        inline ColorRGBA GetPlateColor() const
        {   return m_plateColor;   }

        inline double GetPlateOutlineWidth() const
        {   return m_plateOutlineWidth;   }

        inline ColorRGBA GetPlateOutlineColor() const
        {   return m_plateOutlineColor;   }

    private:
        // for all label types
        unsigned int m_id;
        double      m_fontSize;
        ColorRGBA   m_fontColor;
        std::string m_fontFamily;
        double      m_fontOutlineSize;
        ColorRGBA   m_fontOutlineColor;
        LabelRenderStyleType m_labelType;

        // for contour types
        double      m_contourPadding;

        // for default and padding types
        double      m_offsetHeight;

        // for padding types
        double      m_platePadding;
        double      m_plateOutlineWidth;
        ColorRGBA   m_plateColor;
        ColorRGBA   m_plateOutlineColor;
    };

    // ========================================================================== //
    // ========================================================================== //

    class OSMSCOUT_API RenderStyleConfig
    {
    public:
        RenderStyleConfig(TypeConfig *typeConfig) :
            m_typeConfig(typeConfig),
            m_minDistance(0),
            m_maxDistance(250)
        {
            //TypeInfo list map
            m_numTypes = typeConfig->GetTypes().size();

            m_nodeFillRenderStyles.resize(m_numTypes,NULL);
            m_nodeSymbolRenderStyles.resize(m_numTypes,NULL);
            m_nodeNameLabelRenderStyles.resize(m_numTypes,NULL);

            m_wayLayers.resize(m_numTypes,0);
            m_wayLineRenderStyles.resize(m_numTypes,NULL);
            m_wayNameLabelRenderStyles.resize(m_numTypes,NULL);

            m_areaLayers.resize(m_numTypes,0);
            m_areaFillRenderStyles.resize(m_numTypes,NULL);
            m_areaNameLabelRenderStyles.resize(m_numTypes,NULL);
        }

        ~RenderStyleConfig()
        {
            // clear NODE style data
            for(size_t i=0; i < m_nodeFillRenderStyles.size(); i++)
            {
                if(!(m_nodeFillRenderStyles[i] == NULL))
                {   delete m_nodeFillRenderStyles[i];   }
            }

            for(size_t i=0; i < m_nodeSymbolRenderStyles.size(); i++)
            {
                if(!(m_nodeSymbolRenderStyles[i] == NULL))
                {   delete m_nodeSymbolRenderStyles[i];   }
            }

            for(size_t i=0; i < m_nodeNameLabelRenderStyles.size(); i++)
            {
                if(!(m_nodeNameLabelRenderStyles[i] == NULL))
                {   delete m_nodeNameLabelRenderStyles[i];   }
            }

            // clear WAY style data
            for(size_t i=0; i < m_wayLineRenderStyles.size(); i++)
            {
                if(!(m_wayLineRenderStyles.at(i) == NULL))
                {   delete m_wayLineRenderStyles[i];    }
            }

            for(size_t i=0; i < m_wayNameLabelRenderStyles.size(); i++)
            {
                if(!(m_wayNameLabelRenderStyles.at(i) == NULL))
                {   delete m_wayNameLabelRenderStyles[i];   }
            }


            // clear AREA style data
            for(size_t i=0; i < m_areaFillRenderStyles.size(); i++)
            {
                if(!(m_areaFillRenderStyles.at(i) == NULL))
                {   delete m_areaFillRenderStyles[i];   }
            }

            for(size_t i=0; i < m_areaNameLabelRenderStyles.size(); i++)
            {
                if(!(m_areaNameLabelRenderStyles.at(i) == NULL))
                {   delete m_areaNameLabelRenderStyles[i];   }
            }
        }

        void PostProcess()
        {
            // use sparsely populated property lists
            // to generate a list of unique types
            for(TypeId i=0; i < m_numTypes; i++)
            {
                // nodes MUST have a symbol type spec'd
                if(!(m_nodeFillRenderStyles[i] == NULL))
                {   m_nodeTypes.push_back(i);   }

                // ways MUST have a layer specified
                if(!(m_wayLineRenderStyles[i] == NULL))
                {   m_wayTypes.push_back(i);   }

                // areas MUST have a fill type specified
                if(!(m_areaFillRenderStyles[i] == NULL))
                {   m_areaTypes.push_back(i);  }
            }

            // generate font list
            LabelRenderStyle *labelStyle;

            // m_wayNameLabelRenderStyles
            for(int i=0; i < m_wayTypes.size(); i++)  {
                labelStyle = m_wayNameLabelRenderStyles[m_wayTypes[i]];

                if(!(labelStyle == NULL))
                {   m_listFonts.push_back(labelStyle->GetFontFamily());   }
            }

            // m_areaNameLabelRenderStyles
            for(int i=0; i < m_areaTypes.size(); i++)  {
                labelStyle = m_areaNameLabelRenderStyles[m_areaTypes[i]];

                if(!(labelStyle == NULL))
                {   m_listFonts.push_back(labelStyle->GetFontFamily());   }
            }

            std::vector<std::string>::iterator it;
            std::sort(m_listFonts.begin(),m_listFonts.end());
            it = std::unique(m_listFonts.begin(),m_listFonts.end());
            m_listFonts.resize(it-m_listFonts.begin());
        }


        // Set RendererStyleConfig parameters
        void SetMinDistance(double minDistance)
        {   m_minDistance = minDistance;   }

        void SetMaxDistance(double maxDistance)
        {   m_maxDistance = maxDistance;   }


        // Set NODE info
        void SetNodeFillRenderStyle(TypeId nodeType, FillRenderStyle const &fillRenderStyle)
        {   m_nodeFillRenderStyles[nodeType] = new FillRenderStyle(fillRenderStyle);   }

        void SetNodeSymbolRenderStyle(TypeId nodeType,SymbolRenderStyle const &symbolRenderStyle)
        {   m_nodeSymbolRenderStyles[nodeType] = new SymbolRenderStyle(symbolRenderStyle);   }

        void SetNodeNameLabelRenderStyle(TypeId nodeType,LabelRenderStyle const &labelRenderStyle)
        {   m_nodeNameLabelRenderStyles[nodeType] = new LabelRenderStyle(labelRenderStyle);   }


        // Set WAY info
        void SetWayLayer(TypeId wayType, size_t wayLayer)
        {   m_wayLayers[wayType] = wayLayer;   }

        void SetWayLineRenderStyle(TypeId wayType,LineRenderStyle const &lineRenderStyle)
        {   m_wayLineRenderStyles[wayType] = new LineRenderStyle(lineRenderStyle);   }

        void SetWayNameLabelRenderStyle(TypeId wayType,LabelRenderStyle const &labelRenderStyle)
        {   m_wayNameLabelRenderStyles[wayType] = new LabelRenderStyle(labelRenderStyle);   }


        // Set AREA info
        void SetAreaLayer(TypeId areaType, size_t areaLayer)
        {   m_areaLayers[areaType] = areaLayer;   }

        void SetAreaFillRenderStyle(TypeId areaType, FillRenderStyle const &fillRenderStyle)
        {   m_areaFillRenderStyles[areaType] = new FillRenderStyle(fillRenderStyle);   }

        void SetAreaNameLabelRenderStyle(TypeId areaType, LabelRenderStyle const &labelRenderStyle)
        {   m_areaNameLabelRenderStyles[areaType] = new LabelRenderStyle(labelRenderStyle);   }


        // Get RendererStyleConfig parameters
        TypeConfig* GetTypeConfig() const
        {   return m_typeConfig;   }

        double GetMinDistance() const
        {   return m_minDistance;   }

        double GetMaxDistance() const
        {   return m_maxDistance;   }

        void GetActiveTypes(std::vector<TypeId> &activeTypes) const
        {   // get types that have style data

            activeTypes.clear();
            activeTypes.resize(m_wayTypes.size() +
                               m_areaTypes.size());
            int i=0;

            for(int j=0; j < m_wayTypes.size(); j++)
            {   activeTypes[i] = m_wayTypes[j];  i++;   }

            for(int j=0; j < m_areaTypes.size(); j++)
            {   activeTypes[i] = m_areaTypes[j]; i++;   }
        }

        void GetFontList(std::vector<std::string> &listFonts) const
        {   listFonts = m_listFonts;   }

        // Get NODE info
        void GetNodeTypes(std::vector<TypeId> & nodeTypes) const
        {
            nodeTypes.clear();
            nodeTypes.resize(m_nodeTypes.size());

            for(int i=0; i < m_nodeTypes.size(); i++)
            {   nodeTypes[i] = m_nodeTypes[i];   }
        }

        FillRenderStyle* GetNodeFillRenderStyle(TypeId nodeType) const
        {   return (nodeType < m_numTypes) ? m_nodeFillRenderStyles[nodeType] : NULL;   }

        SymbolRenderStyle* GetNodeSymbolRenderStyle(TypeId nodeType) const
        {   return (nodeType < m_numTypes) ? m_nodeSymbolRenderStyles[nodeType] : NULL;   }

        LabelRenderStyle*  GetNodeNameLabelRenderStyle(TypeId nodeType) const
        {   return (nodeType < m_numTypes) ? m_nodeNameLabelRenderStyles[nodeType] : NULL;   }

        // Get WAY info
        void GetWayTypes(std::vector<TypeId> & wayTypes) const
        {
            wayTypes.clear();
            wayTypes.resize(m_wayTypes.size());

            for(int i=0; i < m_wayTypes.size(); i++)
            {   wayTypes[i] = m_wayTypes[i];   }
        }

        size_t GetWayLayer(TypeId wayType) const
        {   return m_wayLayers[wayType];   }

        size_t GetMaxWayLayer() const
        {
            size_t maxWayLayer = 0;
            for(int i=0; i < m_wayLayers.size(); i++)
            {   maxWayLayer = std::max(maxWayLayer,m_wayLayers[i]);   }

            return maxWayLayer;
        }

        LineRenderStyle* GetWayLineRenderStyle(TypeId wayType) const
        {   return (wayType < m_numTypes) ? m_wayLineRenderStyles[wayType] : NULL;   }

        LabelRenderStyle* GetWayNameLabelRenderStyle(TypeId wayType) const
        {   return (wayType < m_numTypes) ? m_wayNameLabelRenderStyles[wayType] : NULL;   }


        // Get AREA info
        void GetAreaTypes(std::vector<TypeId> & areaTypes) const
        {
            areaTypes.clear();
            areaTypes.resize(m_areaTypes.size());

            for(int i=0; i < m_areaTypes.size(); i++)
            {   areaTypes[i] = m_areaTypes[i];   }
        }

        size_t GetAreaLayer(TypeId areaType) const
        {   return m_areaLayers[areaType];   }

        size_t GetMaxAreaLayer() const
        {
            size_t maxAreaLayer = 0;
            for(int i=0; i < m_areaLayers.size(); i++)
            {   maxAreaLayer = std::max(maxAreaLayer,m_areaLayers[i]);   }

            return maxAreaLayer;
        }

        FillRenderStyle* GetAreaFillRenderStyle(TypeId areaType) const
        {   return (areaType < m_numTypes) ? m_areaFillRenderStyles[areaType] : NULL;   }

        LabelRenderStyle* GetAreaNameLabelRenderStyle(TypeId areaType) const
        {   return (areaType < m_numTypes) ? m_areaNameLabelRenderStyles[areaType] : NULL;   }


    private:
        TypeConfig*                     m_typeConfig;
        unsigned int                    m_numTypes;
        double                          m_minDistance;
        double                          m_maxDistance;

        // NODES
        std::vector<TypeId>             m_nodeTypes;

        // sparsely populated lists
        std::vector<FillRenderStyle*>   m_nodeFillRenderStyles;
        std::vector<SymbolRenderStyle*> m_nodeSymbolRenderStyles;
        std::vector<LabelRenderStyle*>  m_nodeNameLabelRenderStyles;

        // WAYS
        std::vector<TypeId>             m_wayTypes;

        // sparsely populated lists
        std::vector<size_t>             m_wayLayers;
        std::vector<LineRenderStyle*>   m_wayLineRenderStyles;
        std::vector<LabelRenderStyle*>  m_wayNameLabelRenderStyles;


        // AREAS
        std::vector<TypeId>             m_areaTypes;

        // sparsely populated lists
        std::vector<size_t>             m_areaLayers;
        std::vector<FillRenderStyle*>   m_areaFillRenderStyles;
        std::vector<LabelRenderStyle*>  m_areaNameLabelRenderStyles;


        // FONTS
        std::vector<std::string>        m_listFonts;
    };

    // ========================================================================== //
    // ========================================================================== //
}



#endif
