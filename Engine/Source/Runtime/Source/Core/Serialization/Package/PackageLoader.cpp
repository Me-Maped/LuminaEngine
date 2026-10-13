#include "RuntimePCH.h"
#include "PackageLoader.h"

#include "Core/Object/ObjectArray.h"
#include "Core/Object/Package/Package.h"
#include "Core/Versioning/CoreVersion.h"
#include "Memory/Memcpy.h"

namespace Lumina
{
    FArchive& FPackageLoader::operator<<(CObject*& Value)
    {
        FObjectPackageIndex Index;
        FArchive& Ar = *this;
        Ar << Index;
        
        Value = Package->IndexToObject(Index);
        
        return Ar;
    }

    FArchive::FDeferredReaderFactory FPackageLoader::GetDeferredReaderFactory() const
    {
        TSharedPtr<const FPackageNameTable> Table = Names;
        CPackage* Owner = Package;
        const int32 Version = GetFileVersion();
        // Named InBytes rather than Bytes, which would shadow the member this loader holds.
        return [Table, Owner, Version](const TVector<uint8>& InBytes) -> TUniquePtr<FArchive>
        {
            void* Copy = Memory::Malloc(Math::Max<SIZE_T>(InBytes.size(), 1));
            if (!InBytes.empty())
            {
                Memory::Memcpy(Copy, InBytes.data(), InBytes.size());
            }
            TUniquePtr<FPackageLoader> Reader = MakeUnique<FPackageLoader>(MakeShared<FPackageFileBytes>(Copy, (int64)InBytes.size()), Owner);
            Reader->SetNameTable(Table);
            Reader->SetFileVersion(Version);
            return Reader;
        };
    }

    FArchive& FPackageLoader::operator<<(FName& Value)
    {
        if (Names == nullptr || GetFileVersion() < (int32)ELuminaEngineVersion::PACKAGE_NAME_TABLE)
        {
            return FArchive::operator<<(Value);
        }

        SerializePackageName(*this, Value, nullptr, Names.get());
        return *this;
    }

    FArchive& FPackageLoader::operator<<(FObjectHandle& Value)
    {
        FObjectPackageIndex Index;
        FArchive& Ar = *this;
        Ar << Index;

        CObject* Object = Package->IndexToObject(Index);
        Value = GObjectArray.GetHandleByObject(Object);
        Package->LoadObject(Object);
        
        return Ar;    
    }
}
