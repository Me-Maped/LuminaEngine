#include "AnimGraphNode_GetParameter.h"
#include "Localization/Localization.h"
#include "UI/Tools/NodeGraph/Animation/AnimationGraphCompiler.h"

namespace Lumina
{
    FString CAnimGraphNode_GetParameter::GetNodeTitleText() const
    {
        // The parameter name is user data; only the "Get " in front of it is text.
        return ParameterName.IsNone() ? FString(LOC_VIEW_CTX(GetNodeDisplayName(), "node"))
                                      : FString(LOC("Get ")) + ParameterName.ToString();
    }

    void CAnimGraphNode_GetParameter::BuildNode()
    {
        ValuePin = CreateAnimPin("Value", ENodePinDirection::Output, EAnimPinType::Value);
    }

    void CAnimGraphNode_GetParameter::GenerateBytecode(FAnimationGraphCompiler& Compiler)
    {
        Compiler.ValidateParameterKey(ParameterName, this);

        const int32 ParameterIndex = Compiler.AddParameter(ParameterName, EAnimGraphParamType::Float, DefaultValue);
        const uint16 ValueReg = Compiler.EmitLoadParam((uint16)ParameterIndex);

        Compiler.SetPinRegister(ValuePin, ValueReg);
    }
}
