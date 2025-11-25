#include "LiveLinkFaceCSVWriterManager.h"
#include "LiveLinkFaceCSVWriterComponent.h"
#include "ActiveLiveLinkCSVWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

#if WITH_LIVELINKMULTIIPHONE
#include "LLFConnectionManagerLibrary.h"
#include "LLFConnectionManager.h"
#include "LLFDeviceRegistry.h"
#endif

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
        
        FString BaseName = Filename;
        BaseName.RemoveFromEnd(TEXT(".csv"));
        NewWriter->BaseFilename = BaseName;

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
        
        FString BaseName = Filename;
        BaseName.RemoveFromEnd(TEXT(".csv"));
        ActiveWriter->BaseFilename = BaseName;

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

void ULiveLinkFaceCSVWriterManager::RefreshWriters()
{
#if WITH_LIVELINKMULTIIPHONE

    ULLFConnectionManager* ConnMan = ULLFConnectionManagerLibrary::GetConnectionManager();
    if (!ConnMan)
        return;

    ULLFDeviceRegistry* Registry = ConnMan->GetDeviceRegistry();
    if (!Registry)
        return;

    const TArray<FLLFDevice>& Devices = Registry->Devices;

    Registry->RefreshDevices();
    ClearAllWriters();

    if (Devices.Num() <= 1)
    {
        CreateWriterForDevice(Devices.Num() == 1 ? Devices[0] : FLLFDevice());
        return;
    }

    for (const FLLFDevice& Device : Devices)
        CreateWriterForDevice(Device);

    CreateActivePhoneWriter(Registry, TEXT("ActivePhone.csv"));

#else

    // Fallback: plugin not installed
    UE_LOG(LogTemp, Warning, TEXT("CSVWriter: MultiIPhone plugin missing → detecting single iPhone via LiveLink."));

    ClearAllWriters();

    FName Subject = DetectSingleIPhoneSubject();

    if (Subject.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("CSVWriter: No iPhone LiveLink sources detected."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("CSVWriter: Single iPhone detected → %s"), *Subject.ToString());

    CreateWriterForSubject(Subject, Subject.ToString());

#endif
}

bool IsIPhoneSource(const FGuid& SourceGuid, ILiveLinkClient* Client)
{
    if (!Client)
        return false;

    // Get source type/name
    FText SourceType = Client->GetSourceType(SourceGuid);
    FString SourceTypeName = SourceType.ToString();

    // Check if it's an iOS/iPhone source
    // Patterns to look for: "iPhone", "iOS", "Apple ARKit Face", etc.
    if (SourceTypeName.Contains(TEXT("iPhone")) ||
        SourceTypeName.Contains(TEXT("iOS")) ||
        SourceTypeName.Contains(TEXT("Apple ARKit")))
    {
        return true;
    }

    // Alternative: Check subject names for iPhone patterns
    TArray<FLiveLinkSubjectKey> Subjects = Client->GetSubjects(true, false);
    for (const FLiveLinkSubjectKey& Subject : Subjects)
    {
        if (Subject.Source == SourceGuid)
        {
            FString SubjectName = Subject.SubjectName.ToString();
            if (SubjectName.Contains(TEXT("iPhone")) ||
                SubjectName.Contains(TEXT("iOS")))
            {
                return true;
            }
        }
    }

    return false;
}

TArray<FName> GetAllSubjectNamesFromGUID(const FGuid& SourceGuid, ILiveLinkClient* Client)
{
    TArray<FName> SubjectNames;

    if (!Client)
        return SubjectNames;

    // Get all subjects (enabled and disabled)
    TArray<FLiveLinkSubjectKey> AllSubjects = Client->GetSubjects(true, true);

    // Filter subjects that belong to this source
    for (const FLiveLinkSubjectKey& SubjectKey : AllSubjects)
    {
        if (SubjectKey.Source == SourceGuid)
        {
            SubjectNames.Add(SubjectKey.SubjectName.Name);
        }
    }

    return SubjectNames;
}
FName ULiveLinkFaceCSVWriterManager::DetectSingleIPhoneSubject() const
{
    if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
    {
        return NAME_None;
    }

    ILiveLinkClient& Client =
        IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

    TArray<FGuid> SourceGuids = Client.GetSources();

    for (const FGuid& SourceGuid : SourceGuids)
    {
        if (!IsIPhoneSource(SourceGuid, &Client))
            continue;

        TArray<FName> SubjectNames = GetAllSubjectNamesFromGUID(SourceGuid, &Client);

        return SubjectNames[0];
    }

    return NAME_None;
}

void ULiveLinkFaceCSVWriterManager::ApplyNameToFilenames(const FString& Name)
{
    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (!Writer) continue;

        FString NewName = Name + TEXT("_") + Writer->BaseFilename + TEXT(".csv");
        Writer->SetFilename(NewName);
    }
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
        if (Writer)
        {
            // Ensure component is registered
            Writer->ReregisterComponent();
            if (Writer->StartRecording())
            {
                bAnyStarted = true;
            }
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

    SetLastExportedFiles();

    return bSuccess;
}

void ULiveLinkFaceCSVWriterManager::SetLastExportedFiles()
{
    LastExportedFiles.Empty();

    for (ULiveLinkFaceCSVWriterComponent* Writer : Writers)
    {
        if (!Writer) continue;

        if (Writer->ExportFile())
        {
            // Writer->ExportFile() already knows the filename and folder
            FString FullBasePath = Writer->GetSaveFolder() / Writer->GetFilename();
            LastExportedFiles.Add(FullBasePath);

            // If ActivePhone writer, add normalized + switches
            if (Writer->GetName().Contains("ActivePhone"))
            {
                FString BaseNoExt = FPaths::GetBaseFilename(Writer->GetFilename());

                FString Switches = FullBasePath.Replace(TEXT(".csv"), TEXT("_Switches.csv"));
                FString Normalized = FullBasePath.Replace(TEXT(".csv"), TEXT("_Normalized.csv"));

                LastExportedFiles.Add(Switches);
                LastExportedFiles.Add(Normalized);
            }
        }
    }
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
