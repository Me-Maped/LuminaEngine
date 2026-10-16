#include "AnimGraphNode_GetObjectParameter.h"
#include "Localization/Localization.h"
#include "UI/Tools/NodeGraph/Animation/AnimationGraphCompiler.h"

namespace Lumina
{
    FString CAnimGraphNode_GetObjectParameter::GetNodeTitleText() const
    {
        // The parameter name is user data; only the "Get " in front of it is text.
        return ParameterName.IsNone() ? FString(LOC_VIEW_CTX(GetNodeDisplayName(), "node"))
                                      : FString(LOC("Get ")) + ParameterName.ToString();
    }

    void CAnimGraphNode_GetObjectParameter::BuildNode()
    {
        ObjectPin = CreateAnimPin("Object", ENodePinDirection::Output, EAnimPinType::Object);
    }

    void CAnimGraphNode_GetObjectParameter::GenerateBytecode(FAnimationGraphCompiler& Compiler)
    {
        Compiler.ValidateObjectParameterKey(ParameterName, ObjectType, this);

        if (ParameterName.IsNone())
        {
            EdNodeGraph::FError NodeWarning;
            NodeWarning.Name        = "Unbound Object Parameter";
            NodeWarning.Description = "Get Object Parameter has no field assigned, so it will always evaluate to nothing.";
            NodeWarning.Node        = this;
            Compiler.AddWarning(NodeWarning);
        }

        const int32 ParamIndex = Compiler.AddObjectParameter(ParameterName, ObjectType);
        const uint16 ObjectReg = Compiler.EmitLoadObjectParam((uint16)ParamIndex);

        Compiler.SetPinRegister(ObjectPin, ObjectReg);
    }
}
