#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/** Saves controller configuration without deleting an existing file on failure. */
namespace ImperialProfileStorage
{
    bool Save(const FString& Path, const FString& Contents);
    /** Nonblocking, cooperative lock for one RadioProfile read/merge/save transaction.
     * Older versions and unrelated external writers do not participate. */
    class FRadioProfileWriteLock
    {
    public:
        explicit FRadioProfileWriteLock(const FString& Path);
        ~FRadioProfileWriteLock();
        FRadioProfileWriteLock(const FRadioProfileWriteLock&) = delete;
        FRadioProfileWriteLock& operator=(const FRadioProfileWriteLock&) = delete;
        bool IsHeld() const { return Handle != nullptr; }
    private:
        void* Handle = nullptr;
    };
    /** Reads an existing schema-1 store without replacing malformed profile data.
     *  Missing files receive empty in-memory objects; this function never writes. */
    bool LoadRadioProfilesForUpdate(const FString& Path, TSharedPtr<FJsonObject>& Root, TSharedPtr<FJsonObject>& Profiles);
}
