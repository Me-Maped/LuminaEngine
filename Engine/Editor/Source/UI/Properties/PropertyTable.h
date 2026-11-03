#pragma once

#include "Containers/Function.h"
#include "Containers/Span.h"
#include <initializer_list>
#include "Containers/String.h"
#include "Containers/Vector.h"
#include "Core/Delegates/Delegate.h"
#include "Core/Reflection/PropertyChangedEvent.h"
#include "Core/Reflection/PropertyCustomization/PropertyCustomization.h"
#include "Memory/SmartPtr.h"
#include "Scripting/ManagedTypeRegistry.h"
#include "imgui.h"

namespace Lumina::PropertyTableInternal
{
    class FCategoryPropertyRow;
}

namespace Lumina
{
    class CObject;
    class CStruct;
    class FProperty;
    struct FInstancedStruct;

    using FPropertyChangedEventFn = TFunction<void(const FPropertyChangedEvent&)>;
    using FPropertyMultiValueFn = TFunction<bool(FProperty*)>;
    using FPropertyTrailingControlFn = TFunction<void(FProperty*)>;

    struct FPropertyChangedEventCallbacks
    {
        CStruct*                    Type = nullptr;
        void*                       Instance = nullptr;
        FPropertyChangedEventFn     PreChangeCallback;
        FPropertyChangedEventFn     PostChangeCallback;
        FPropertyChangedEventFn     StartChangeCallback;
        FPropertyChangedEventFn     FinishChangeCallback;
        FPropertyMultiValueFn       IsMultiValueFn;
        FPropertyTrailingControlFn  TrailingControlFn;
        float                       TrailingControlWidth = 0.0f;
        bool                        bEditMultipleValues = false;
    };

    class EDITOR_API FPropertyTable : public Scripting::IScriptReloadListener
    {
    public:

        FPropertyTable();
        FPropertyTable(void* InObject, CStruct* InType);
        FPropertyTable(void* InObject, CStruct* InType, void* InDefaultObject);
        explicit FPropertyTable(CObject* InObject);
        ~FPropertyTable() override;

        FPropertyTable(const FPropertyTable&) = delete;
        FPropertyTable(FPropertyTable&&) = delete;
        FPropertyTable& operator=(const FPropertyTable&) = delete;
        FPropertyTable& operator=(FPropertyTable&&) = delete;

        // Out-of-view rows reserve their last height, and this frame draws them all so no height stays stale.
        static bool IsDetailsRemeasureFrame() { return ImGui::GetFrameCount() % 32 == 0; }

        void DrawTree(bool bReadOnly = false);
        void DrawTree(const FPropertyDrawArgs& Args);
        void UpdateRows();
        void MarkDirty();

        void SetObject(void* InObject, CStruct* StructType);
        void SetObject(void* InObject, CStruct* StructType, void* InDefaultObject);
        CStruct* GetType() const { return Struct; }
        void* GetObject() const { return Object; }
        void* GetDefaultObject() const { return DefaultObject; }

        void SetPreEditCallback(FPropertyChangedEventFn Callback);
        void SetPostEditCallback(FPropertyChangedEventFn Callback);
        void SetStartEditCallback(FPropertyChangedEventFn Callback);
        void SetFinishEditCallback(FPropertyChangedEventFn Callback);
        void SetMultiValueFn(FPropertyMultiValueFn Fn);
        void SetEditMultipleValues(bool bEdit);
        void SetTrailingControl(FPropertyTrailingControlFn Fn, float Width);

        // Test-support: a read-only snapshot of one row in the built tree, so a suite can assert search
        // and expansion behavior without reaching into the row implementation, which lives in the .cpp.
        struct FRowProbe
        {
            FString InternalName;                   // Reflected member name; empty for rows that have none.
            FString FilterLabel;                    // The label the search box matches, in the active language.
            bool    bPassesFilter = false;
            bool    bExpanded = false;
            size_t  ChildCount = 0;
            size_t  Depth = 0;                      // 0 for a category header, +1 per level below it.
        };
        // Builds the tree, applies the current filter, and reports every row parents-first.
        void ProbeRows(TVector<FRowProbe>& Out);
        // Opens or closes one row by reflected member name, for a suite that needs a chosen start state.
        bool SetRowExpanded(FStringView InternalName, bool bExpanded);

        void SetContext(const FPropertyEditContext* InContext) { Context = InContext; }
        void SetShowSearchBar(bool bShow) { bShowSearchBar = bShow; }
        void SetOwnerHeading(const FName& InHeading) { OwnerHeading = InHeading; }
        void SetSearchText(FStringView Text);
        // Owning aliases supplied by the enclosing component section. A query term may match one of
        // these as well as a row, so the panel's search box can find "MovementSystem 帧率".
        void SetSearchContext(std::initializer_list<FStringView> Fields);
        // Drops the query and the context, restoring every row's unfiltered visibility.
        void ClearSearch();
        bool PrepareAndTestFilter();

    private:

        void OnScriptsReloaded(const Scripting::FScriptReload& Reload) override;

        void Bind(void* InObject, CStruct* StructType, void* InDefaultObject);
        void DropRows();
        void EnsureTreeBuilt();
        void RebuildTree();
        void ApplyFilterState();
        void DrawCustomization(const FPropertyDrawArgs& Args);
        PropertyTableInternal::FCategoryPropertyRow* FindOrCreateCategoryRow(const FName& CategoryName);

        FPropertyChangedEventCallbacks                                      Callbacks;
        TVector<TUniquePtr<PropertyTableInternal::FCategoryPropertyRow>>    Categories;
        TSharedPtr<IPropertyTypeCustomization>                              Customization;
        TSharedPtr<FPropertyHandle>                                         CustomizationHandle;
        ImGuiTextFilter                                                     PropertyFilter;
        TVector<FString>                                                    SearchContext;
        FName                                                               OwnerHeading;
        FDelegateHandle                                                     ScriptsWillReloadHandle;
        const FPropertyEditContext*                                         Context = nullptr;
        CStruct*                                                            Struct = nullptr;
        void*                                                               Object = nullptr;
        void*                                                               DefaultObject = nullptr;
        bool                                                                bDirty = true;
        bool                                                                bShowSearchBar = true;
        bool                                                                bWasFiltering = false;
        bool                                                                bCustomizationEditActive = false;
    };

    // The editor for a value that fits one cell, a leaf or a customized struct; null when it needs member rows.
    TSharedPtr<IPropertyTypeCustomization> MakeInlinePropertyCustomization(FProperty* Prop);

    bool DrawInstancedStructEditor(const char* StrId, FInstancedStruct& Value, CStruct* BaseStruct, FPropertyTable& Table);
}
