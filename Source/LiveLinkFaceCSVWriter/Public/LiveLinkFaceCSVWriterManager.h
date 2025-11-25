#pragma once
#include "CoreMinimal.h"

#include "EditorSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LiveLinkFaceCSVWriterManager.generated.h"

// Forward declarations
class ULiveLinkFaceCSVWriterComponent;
class UActiveLiveLinkCSVWriter;
class UWorld;
struct FLLFDevice;

/** Event delegates */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecordingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecordingStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecordingTick, float, DeltaTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExportComplete, bool, bSuccess, int32, NumFilesExported);

/**
 * Global subsystem for managing LiveLink CSV recording
 * 
 * IN EDITOR: Access via Get Editor Subsystem → LiveLinkFaceCSVWriterManager
 * 
 * Coordinates multiple CSV writer components and provides global access
 */

UCLASS()
class LIVELINKFACECSVWRITER_API ULiveLinkFaceCSVWriterManager : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ========== Writer Management ==========

    /** Create a writer component for a specific subject */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    ULiveLinkFaceCSVWriterComponent* CreateWriterForSubject(const FName& SubjectName, const FString& Filename);

    /** Create a writer component for a device from the registry */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    ULiveLinkFaceCSVWriterComponent* CreateWriterForDevice(const FLLFDevice& Device);

    /** Create an active phone writer that switches between devices */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    UActiveLiveLinkCSVWriter* CreateActivePhoneWriter(ULLFDeviceRegistry* DeviceRegistry, const FString& Filename = TEXT("ActivePhone.csv"));

    /** Manually register an existing writer component */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    void RegisterWriter(ULiveLinkFaceCSVWriterComponent* Writer);

    /** Unregister a writer component */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    void UnregisterWriter(ULiveLinkFaceCSVWriterComponent* Writer);

    /** Get all registered writers */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    TArray<ULiveLinkFaceCSVWriterComponent*> GetAllWriters() const { return Writers; }

    /** Remove all writers */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    void ClearAllWriters();

    UFUNCTION(BlueprintCallable, Category = "LiveLink CSV Writer")
    void RefreshWriters();

    UFUNCTION(BlueprintCallable, Category = "LiveLink CSV Writer")
    FName DetectSingleIPhoneSubject() const;

    UFUNCTION(BlueprintCallable)
    void ApplyNameToFilenames(const FString& Name);

    // ========== Recording Control ==========

    /** Start recording on all registered writers */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    bool StartRecording();

    /** Stop recording on all registered writers */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    void StopRecording();

    /** Check if any writer is currently recording */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    bool IsRecording() const { return bIsRecording; }

    void SetLastExportedFiles();

    /** Export all recorded data to files */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    bool ExportAllFiles();

    // ========== Configuration ==========

    /** Set export folder for all current and future writers */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    void SetExportPath(const FString& FolderPath);

    /** Get the current export path */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer Manager")
    FString GetExportPath() const { return ExportPath; }

    // ========== Events ==========

    /** Broadcast when recording starts */
    UPROPERTY(BlueprintAssignable, Category="LiveLink CSV Writer Manager")
    FOnRecordingStarted OnRecordingStarted;

    /** Broadcast when recording stops */
    UPROPERTY(BlueprintAssignable, Category="LiveLink CSV Writer Manager")
    FOnRecordingStopped OnRecordingStopped;

    /** Broadcast each tick during recording */
    UPROPERTY(BlueprintAssignable, Category="LiveLink CSV Writer Manager")
    FOnRecordingTick OnRecordingTick;

    /** Broadcast when export completes */
    UPROPERTY(BlueprintAssignable, Category="LiveLink CSV Writer Manager")
    FOnExportComplete OnExportComplete;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> LastExportedFiles;

private:
    /** Tick function called by timer */
    void Tick();

    /** Get the world context for this subsystem */
    UWorld* GetWorldContext() const;

    /** All registered writer components */
    UPROPERTY()
    TArray<ULiveLinkFaceCSVWriterComponent*> Writers;

    /** Export folder path for all writers */
    UPROPERTY()
    FString ExportPath;

    /** Are we currently recording? */
    bool bIsRecording;

    /** Timer handle for ticking */
    FTimerHandle TickTimerHandle;
};
