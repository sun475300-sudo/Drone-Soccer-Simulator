#include "ImperialProfileStorage.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

ImperialProfileStorage::FRadioProfileWriteLock::FRadioProfileWriteLock(const FString& Path)
{
#if PLATFORM_WINDOWS
    if (Path.IsEmpty()) return;
    const FString FullPath=FPaths::ConvertRelativePathToFull(Path);
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath),true)) return;
    // OPEN_ALWAYS preserves an existing lock file. Share mode zero rejects another
    // participating process immediately; we never delete the sidecar after close.
    const FString LockPath=FullPath+TEXT(".lock");
    HANDLE Opened=::CreateFileW(*LockPath,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if (Opened==INVALID_HANDLE_VALUE) return;
    FILE_ATTRIBUTE_TAG_INFO Info{};
    if (!::GetFileInformationByHandleEx(Opened,FileAttributeTagInfo,&Info,sizeof(Info)) ||
        (Info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) != 0)
    {
        ::CloseHandle(Opened); return;
    }
    Handle=Opened;
#endif
}
ImperialProfileStorage::FRadioProfileWriteLock::~FRadioProfileWriteLock()
{
#if PLATFORM_WINDOWS
    if (Handle) ::CloseHandle(static_cast<HANDLE>(Handle));
#endif
}

bool ImperialProfileStorage::Save(const FString& Path, const FString& Contents)
{
#if PLATFORM_WINDOWS
    if (Path.IsEmpty()) return false;
    const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true)) return false;
    // A unique sibling stays on the same volume and cannot collide with another
    // save. Never use FileManager::Move here: it pre-deletes the old destination.
    const FString TempPath = FullPath + TEXT(".tmp-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const bool Saved = FFileHelper::SaveStringToFile(Contents, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        ::MoveFileExW(*TempPath, *FullPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!Saved) IFileManager::Get().Delete(*TempPath, false, true, true);
    return Saved;
#else
    // The controller backend is Windows-only. Do not use a destructive fallback.
    return false;
#endif
}


bool ImperialProfileStorage::LoadRadioProfilesForUpdate(const FString& Path, TSharedPtr<FJsonObject>& Root, TSharedPtr<FJsonObject>& Profiles)
{
    Root.Reset(); Profiles.Reset();
    if (Path.IsEmpty()) return false;
    if (!IFileManager::Get().FileExists(*Path))
    {
        Root=MakeShared<FJsonObject>(); Profiles=MakeShared<FJsonObject>();
        return true;
    }
    FString Text;
    double Version=0;
    const TSharedPtr<FJsonObject>* StoredProfiles=nullptr;
    if (!FFileHelper::LoadFileToString(Text,*Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid() ||
        !Root->TryGetNumberField(TEXT("version"),Version) || !FMath::IsFinite(Version) || Version!=1.0 ||
        !Root->TryGetObjectField(TEXT("profiles"),StoredProfiles) || !StoredProfiles || !StoredProfiles->IsValid())
    {
        Root.Reset(); return false;
    }
    Profiles=*StoredProfiles;
    return true;
}
