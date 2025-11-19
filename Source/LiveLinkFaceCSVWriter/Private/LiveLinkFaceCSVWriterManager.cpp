#include "LiveLinkFaceCSVWriterManager.h"
#include "LiveLinkFaceCSVWriterComponent.h"
#include "ActiveLiveLinkCSVWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

void ULiveLinkFaceCSVWriterManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    ExportPath = FPaths::ProjectSavedDir() / TEXT("LiveLinkExports");
    bIsRecording = false;
    
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Initialized"));
}

void ULiveLinkFaceCSVWriterManager::Deinitialize()
{
    // Clean up if recording
    if (bIsRecording)
    {
        StopRecording();
        ExportAllFiles();
    }
    
    // Clear writers
    ClearAllWriters();
    
    Super::Deinitialize();
    
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Deinitialized"));
}

UWorld* ULiveLinkFaceCSVWriterManager::GetWorldContext() const
{
#if WITH_EDITOR
    // In editor, get the editor world
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
#endif
    
    // At runtime, get from game instance
    if (UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter()))
    {
        return GameInstance->GetWorld();
    }
    
    return nullptr;
}

void ULiveLinkFaceCSVWriterManager::Tick()
{
    if (bIsRecording)
    {
        UWorld* World = GetWorldContext();
        float DeltaTime = World ? World->GetDeltaSeconds() : 0.0166f; // fallback to ~60fps
        
        OnRecordingTick.Broadcast(DeltaTime);
    }
}

ULiveLinkFaceCSVWriterComponent* ULiveLinkFaceCSVWriterManager::CreateWriterForSubject(
    const FName& SubjectName, const FString& Filename)
{
    if (SubjectName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot create writer - invalid subject name"));
        return nullptr;
    }

    UWorld* World = GetWorldContext();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot create writer - no world context"));
        return nullptr;
    }

    // Create the component
    ULiveLinkFaceCSVWriterComponent* NewWriter = NewObject<ULiveLinkFaceCSVWriterComponent>(
        this, 
        ULiveLinkFaceCSVWriterComponent::StaticClass(),
        *FString::Printf(TEXT("Writer_%s"), *SubjectName.ToString())
    );

    if (NewWriter)
    {
        NewWriter->SetSubjectName(SubjectName);
        NewWriter->SetFilename(Filename);
        NewWriter->SetSaveFolder(ExportPath);
        
        // Register with world
        NewWriter->RegisterComponentWithWorld(World);
        
        Writers.Add(NewWriter);
        
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Created writer #%d for subject '%s', filename '%s', folder '%s'"), 
            Writers.Num(), *SubjectName.ToString(), *Filename, *ExportPath);
    }

    return NewWriter;
}

#if WITH_LIVELINKMULTIIPHONE
ULiveLinkFaceCSVWriterComponent* ULiveLinkFaceCSVWriterManager::CreateWriterForDevice(const FLLFDevice& Device)
{
    if (Device.SubjectName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot create writer - device has no subject name"));
        return nullptr;
    }

    // Use DeviceID if available, otherwise use SubjectName to ensure unique filename
    FString Filename;
    if (Device.DeviceID != NAME_None && !Device.DeviceID.ToString().IsEmpty())
    {
        Filename = FString::Printf(TEXT("Device_%s.csv"), *Device.DeviceID.ToString());
    }
    else
    {
        Filename = FString::Printf(TEXT("Device_%s.csv"), *Device.SubjectName);
        UE_LOG(LogTemp, Warning, TEXT("LiveLink CSV Writer Manager: Device has no ID, using subject name for filename: %s"), *Filename);
    }

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Creating writer for device - ID: %s, Subject: %s, Filename: %s"), 
        *Device.DeviceID.ToString(), *Device.SubjectName, *Filename);
    
    return CreateWriterForSubject(FName(*Device.SubjectName), Filename);
}

UActiveLiveLinkCSVWriter* ULiveLinkFaceCSVWriterManager::CreateActivePhoneWriter(
    ULLFDeviceRegistry* DeviceRegistry, const FString& Filename)
{
    if (!DeviceRegistry)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot create active writer - no registry provided"));
        return nullptr;
    }

    UWorld* World = GetWorldContext();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot create active writer - no world context"));
        return nullptr;
    }

    // Create the active writer component
    UActiveLiveLinkCSVWriter* ActiveWriter = NewObject<UActiveLiveLinkCSVWriter>(
        this,
        UActiveLiveLinkCSVWriter::StaticClass(),
        TEXT("ActivePhoneWriter")
    );

    if (ActiveWriter)
    {
        ActiveWriter->SetDeviceRegistry(DeviceRegistry);
        ActiveWriter->SetFilename(Filename);
        ActiveWriter->SetSaveFolder(ExportPath);
        
        // Register with world
        ActiveWriter->RegisterComponentWithWorld(World);
        
        Writers.Add(ActiveWriter);
        
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Created active phone writer"));
    }

    return ActiveWriter;
}
#endif

void ULiveLinkFaceCSVWriterManager::RegisterWriter(ULiveLinkFaceCSVWriterComponent* Writer)
{
    if (!Writer)
    {
        return;
    }

    if (!Writers.Contains(Writer))
    {
        Writers.Add(Writer);
        Writer->SetSaveFolder(ExportPath);
        
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Registered writer '%s'"), 
            *Writer->GetName());
    }
}

void ULiveLinkFaceCSVWriterManager::UnregisterWriter(ULiveLinkFaceCSVWriterComponent* Writer)
{
    if (Writer)
    {
        Writers.Remove(Writer);
        
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Unregistered writer '%s'"), 
            *Writer->GetName());
    }
}

void ULiveLinkFaceCSVWriterManager::ClearAllWriters()
{
    // Stop recording if active
    if (bIsRecording)
    {
        StopRecording();
    }

    // Destroy all writer components
    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (Writer)
        {
            Writer->DestroyComponent();
        }
    }

    Writers.Empty();
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Cleared all writers"));
}

bool ULiveLinkFaceCSVWriterManager::StartRecording()
{
    if (Writers.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer Manager: Cannot start - no writers registered"));
        return false;
    }

    bool bAnyStarted = false;

    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (Writer && Writer->StartRecording())
        {
            bAnyStarted = true;
        }
    }

    if (bAnyStarted)
    {
        bIsRecording = true;
        
        // Set up tick timer
        if (UWorld* World = GetWorldContext())
        {
            World->GetTimerManager().SetTimer(
                TickTimerHandle,
                this,
                &ULiveLinkFaceCSVWriterManager::Tick,
                0.0166f, // ~60fps
                true     // Loop
            );
        }
        
        OnRecordingStarted.Broadcast();
        
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Started recording (%d writers)"), Writers.Num());
    }

    return bAnyStarted;
}

void ULiveLinkFaceCSVWriterManager::StopRecording()
{
    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (Writer)
        {
            Writer->StopRecording();
        }
    }

    bIsRecording = false;
    
    // Clear timer
    if (UWorld* World = GetWorldContext())
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
    }
    
    OnRecordingStopped.Broadcast();
    
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Stopped recording"));
}

bool ULiveLinkFaceCSVWriterManager::ExportAllFiles()
{
    if (Writers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("LiveLink CSV Writer Manager: No writers to export"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Exporting from %d writers:"), Writers.Num());
    for (int32 i = 0; i < Writers.Num(); i++)
    {
        if (Writers[i])
        {
            UE_LOG(LogTemp, Log, TEXT("  Writer %d: %s (Subject: %s, File: %s)"), 
                i, *Writers[i]->GetName(), *Writers[i]->GetSubjectName().ToString(), *Writers[i]->GetFilename());
        }
    }

    int32 SuccessCount = 0;

    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (Writer && Writer->ExportFile())
        {
            SuccessCount++;
        }
    }

    bool bSuccess = (SuccessCount == Writers.Num());
    OnExportComplete.Broadcast(bSuccess, SuccessCount);

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Exported %d/%d writers"), 
        SuccessCount, Writers.Num());

    return bSuccess;
}

void ULiveLinkFaceCSVWriterManager::SetExportPath(const FString& FolderPath)
{
    if (FPaths::IsRelative(FolderPath))
    {
        ExportPath = FPaths::ProjectSavedDir() / FolderPath;
    }
    else
    {
        ExportPath = FolderPath;
    }

    ExportPath = FPaths::GetPath(ExportPath / TEXT(""));

    // Update all existing writers
    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (Writer)
        {
            Writer->SetSaveFolder(ExportPath);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer Manager: Export path set to '%s'"), *ExportPath);
}
