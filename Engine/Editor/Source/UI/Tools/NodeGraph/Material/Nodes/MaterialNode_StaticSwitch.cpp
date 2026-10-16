#include "MaterialNode_StaticSwitch.h"
#include "Localization/Localization.h"

#include "Core/Object/Cast.h"
#include "UI/Tools/NodeGraph/Material/MaterialCompiler.h"
#include "UI/Tools/NodeGraph/Material/MaterialOutput.h"

#include "MaterialNodePinHelpers.h"

namespace Lumina
{
    void CMaterialExpression_StaticSwitch::BuildNode()
    {
        Super::BuildNode();
        True  = MakeIn(this, "True");
        False = MakeIn(this, "False");
    }

    CEdNodeGraphPin* CMaterialExpression_StaticSwitch::GetRerouteSourcePin() const
    {
        return bResolvedValue ? True : False;
    }

    void CMaterialExpression_StaticSwitch::DrawNodeBody()
    {
        if (bDynamic && !ParameterName.IsNone())
        {
            ImGui::TextColored(ImVec4(0.62f, 0.55f, 0.85f, 1.0f), "%s", ParameterName.c_str());
        }

        if (ImGui::Checkbox(bDynamic ? LOC("Default") : LOC("Value"), &bDefaultValue))
        {
            NotifyValueEdited();
        }
    }

    void CMaterialExpression_StaticSwitch::DrawContextMenu()
    {
        const char* MenuItem = bDynamic ? LOC("Make Static Switch") : LOC("Make Switch Parameter");
        if (ImGui::MenuItem(MenuItem))
        {
            bDynamic = !bDynamic;
            if (bDynamic && ParameterName.IsNone())
            {
                ParameterName = FString(GetNodeDisplayName()) + "_Param";
            }
            NotifyValueEdited();
        }
    }
}
