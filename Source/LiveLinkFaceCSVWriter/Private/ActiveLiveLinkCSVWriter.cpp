#include "ActiveLiveLinkCSVWriter.h"
#include "Engine/Engine.h"

#if WITH_LIVELINKMULTIIPHONE
#include "LLFConnectionManagerLibrary.h"
#include "LLFConnectionManager.h"
#include "LLFDeviceRegistry.h"
#endif

UActiveLiveLinkCSVWriter::UActiveLiveLinkCSVWriter()
{
#if WITH_LIVELINKMULTIIPHONE
    DeviceRegistry = nullptr;
#endif

    RecordingStartTime = 0.0;
    CurrentRecordingTime = 0.0;
    LastDeviceID = NAME_None;
    bNormalizedHeaderWritten = false;
}

void UActiveLiveLinkCSVWriter::BeginPlay()
{
    Super::BeginPlay();

#if WITH_LIVELINKMULTIIPHONE
    // // If we have a registry, bind to its event
    // if (DeviceRegistry)
    // {
    //     DeviceRegistry->OnActiveIPhoneChanged.AddDynamic(this, &UActiveLiveLinkCSVWriter::OnActiveDeviceChanged);
    //     
    //     // Set initial subject from active device
    //     if (DeviceRegistry->ActiveDevice.DeviceID != NAME_None)
    //     {
    //         SetSubjectName(FName(*DeviceRegistry->ActiveDevice.SubjectName));
    //     }
    // }
#endif
}

void UActiveLiveLinkCSVWriter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_LIVELINKMULTIIPHONE
    if (DeviceRegistry)
    {
        DeviceRegistry->OnActiveIPhoneChanged.RemoveDynamic(this, &UActiveLiveLinkCSVWriter::OnActiveDeviceChanged);
    }
#endif

    Super::EndPlay(EndPlayReason);
}

#if WITH_LIVELINKMULTIIPHONE
void UActiveLiveLinkCSVWriter::SetDeviceRegistry(ULLFDeviceRegistry* InRegistry)
{
    if (DeviceRegistry)
    {
        DeviceRegistry->OnActiveIPhoneChanged.RemoveDynamic(this, &UActiveLiveLinkCSVWriter::OnActiveDeviceChanged);
    }

    DeviceRegistry = InRegistry;

    if (DeviceRegistry)
    {
        DeviceRegistry->OnActiveIPhoneChanged.AddDynamic(this, &UActiveLiveLinkCSVWriter::OnActiveDeviceChanged);

        if (DeviceRegistry->ActiveDevice.DeviceID != NAME_None)
        {
            SetSubjectName(FName(*DeviceRegistry->ActiveDevice.SubjectName));
        }
    }
}

void UActiveLiveLinkCSVWriter::OnActiveDeviceChanged(FName DeviceID)
{
    if (!DeviceRegistry)
    {
        return;
    }

    // Find the device in the registry
    for (const FLLFDevice& Device : DeviceRegistry->Devices)
    {
        if (Device.DeviceID == DeviceID)
        {
            // Switch to the new subject
            SetSubjectName(FName(*Device.SubjectName));
            UE_LOG(LogTemp, Warning, TEXT("Active LiveLink CSV Writer: Switched to device %s (subject: %s)"), 
                *DeviceID.ToString(), *Device.SubjectName);
            
            // Note: We don't reset the CSV - we want continuous recording across switches
            // The header is already written, we just continue capturing frames
            break;
        }
    }
}
#endif

bool UActiveLiveLinkCSVWriter::StartRecording()
{
#if WITH_LIVELINKMULTIIPHONE
    if (!DeviceRegistry)
    {
        UE_LOG(LogTemp, Error, TEXT("Active LiveLink CSV Writer: Cannot start - no DeviceRegistry set"));
        return false;
    }

    if (DeviceRegistry->ActiveDevice.DeviceID == NAME_None)
    {
        UE_LOG(LogTemp, Error, TEXT("Active LiveLink CSV Writer: Cannot start - no active device in registry"));
        return false;
    }

    // Ensure we're using the current active device's subject
    SetSubjectName(FName(*DeviceRegistry->ActiveDevice.SubjectName));
    RecordingStartTime = FPlatformTime::Seconds();
    CurrentRecordingTime = 0.0;
    NormalizedCSVRows.Empty();
    DeviceSwitchLog.Empty();
    LastDeviceID = DeviceRegistry->ActiveDevice.DeviceID;
    bNormalizedHeaderWritten = false;
    
    // Log the initial device
    DeviceSwitchLog.Add(TPair<double, FName>(0.0, LastDeviceID));
    UE_LOG(LogTemp, Log, TEXT("Active Phone Writer: Started with device %s"), *LastDeviceID.ToString());
#else
    UE_LOG(LogTemp, Error, TEXT("Active LiveLink CSV Writer: Cannot start - LiveLinkMultiIPhone plugin not available"));
    return false;
#endif

    return Super::StartRecording();
}

void UActiveLiveLinkCSVWriter::StopRecording()
{
    Super::StopRecording();
}

bool UActiveLiveLinkCSVWriter::InitializeCSVHeader()
{
    // Call parent to initialize the original CSV with phone timecodes
    bool bSuccess = Super::InitializeCSVHeader();
    
    if (bSuccess && !bNormalizedHeaderWritten)
    {
        // Create header for normalized CSV (only once, even if device switches)
        // Copy the header from parent but replace "Timecode" with "RecordingTime,Timecode"
        if (CSVRows.Num() > 0)
        {
            FString OriginalHeader = CSVRows[0];
            FString NormalizedHeader = TEXT("RecordingTime,") + OriginalHeader;
            NormalizedCSVRows.Add(NormalizedHeader);
            bNormalizedHeaderWritten = true;
            
            UE_LOG(LogTemp, Log, TEXT("Active Phone Writer: Initialized normalized CSV header"));
        }
    }
    
    return bSuccess;
}

void UActiveLiveLinkCSVWriter::CaptureFrame()
{
    // Update recording time
    CurrentRecordingTime = FPlatformTime::Seconds() - RecordingStartTime;
    
#if WITH_LIVELINKMULTIIPHONE
    // Check if device switched
    if (DeviceRegistry && DeviceRegistry->ActiveDevice.DeviceID != LastDeviceID)
    {
        LastDeviceID = DeviceRegistry->ActiveDevice.DeviceID;
        DeviceSwitchLog.Add(TPair<double, FName>(CurrentRecordingTime, LastDeviceID));
        
        UE_LOG(LogTemp, Warning, TEXT("Active Phone Writer: Device switched to %s at %.3f seconds"), 
            *LastDeviceID.ToString(), CurrentRecordingTime);
    }
#endif
    
    // Track how many rows before parent call (to detect if parent added a row)
    int32 RowCountBefore = CSVRows.Num();
    
    // Call parent to capture with original phone timecode (may skip duplicates)
    Super::CaptureFrame();
    
    // Only add normalized row if parent actually added a new row (wasn't a duplicate)
    if (CSVRows.Num() > RowCountBefore)
    {
        // Get the last captured row (has original timecode)
        FString LastRow = CSVRows.Last();
        
        // Format normalized timecode from recording time
        FString NormalizedTimecode = FormatNormalizedTimecode(CurrentRecordingTime);
        
        // Prepend RecordingTime to the row
        FString NormalizedRow = FString::Printf(TEXT("%.6f,%s"), CurrentRecordingTime, *NormalizedTimecode);
        
        // Append the rest of the row (skip the original timecode column)
        int32 FirstCommaIndex;
        if (LastRow.FindChar(',', FirstCommaIndex))
        {
            FString DataColumns = LastRow.RightChop(FirstCommaIndex + 1); // Everything after first comma
            NormalizedRow += TEXT(",") + DataColumns;
        }
        
        NormalizedCSVRows.Add(NormalizedRow);
    }
}

FString UActiveLiveLinkCSVWriter::FormatNormalizedTimecode(double Seconds) const
{
    int32 Hours = FMath::FloorToInt(Seconds / 3600.0);
    Seconds -= Hours * 3600.0;
    
    int32 Minutes = FMath::FloorToInt(Seconds / 60.0);
    Seconds -= Minutes * 60.0;
    
    int32 Secs = FMath::FloorToInt(Seconds);
    double Fraction = Seconds - Secs;
    
    // Assuming 30fps for frames
    int32 Frames = FMath::FloorToInt(Fraction * 30.0);
    int32 Millis = FMath::RoundToInt((Fraction - (Frames / 30.0)) * 1000.0);
    
    return FString::Printf(TEXT("%02d:%02d:%02d:%02d.%03d"), 
        Hours, Minutes, Secs, Frames, Millis);
}

bool UActiveLiveLinkCSVWriter::ExportFile()
{
    // Export original file with phone timecodes
    bool bOriginalSuccess = Super::ExportFile();
    
    // Export normalized file
    bool bNormalizedSuccess = ExportNormalizedFile();
    
    // Export switches log
    bool bSwitchesSuccess = ExportSwitchesLog();
    
    return bOriginalSuccess && bNormalizedSuccess && bSwitchesSuccess;
}

bool UActiveLiveLinkCSVWriter::ExportNormalizedFile()
{
    if (NormalizedCSVRows.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Active Phone Writer: No normalized data to export"));
        return false;
    }
    
    // Create filename by inserting "_Normalized" before .csv
    FString LocalBaseName = Filename;
    LocalBaseName.RemoveFromEnd(TEXT(".csv"));
    FString NormalizedFilename = LocalBaseName + TEXT("_Normalized.csv");
    
    const FString FullPath = ExportFolder / NormalizedFilename;
    const FString Dir = FPaths::GetPath(FullPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
    {
        PF.CreateDirectoryTree(*Dir);
    }
    
    const FString Content = FString::Join(NormalizedCSVRows, TEXT("\n"));
    if (FFileHelper::SaveStringToFile(Content, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("Active Phone Writer: Exported %d normalized rows to %s"),
            NormalizedCSVRows.Num(), *FullPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Active Phone Writer: Failed to save normalized file to %s"), *FullPath);
        return false;
    }
}

bool UActiveLiveLinkCSVWriter::ExportSwitchesLog()
{
    if (DeviceSwitchLog.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Active Phone Writer: No device switches to export"));
        return false;
    }
    
    // Create filename
    FString LocalBaseName = Filename;
    LocalBaseName.RemoveFromEnd(TEXT(".csv"));
    FString SwitchesFilename = LocalBaseName + TEXT("_Switches.csv");
    
    const FString FullPath = ExportFolder / SwitchesFilename;
    const FString Dir = FPaths::GetPath(FullPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
    {
        PF.CreateDirectoryTree(*Dir);
    }
    
    // Build CSV content
    TArray<FString> SwitchRows;
    SwitchRows.Add(TEXT("RecordingTime,Timecode,DeviceID")); // Header
    
    for (const TPair<double, FName>& Switch : DeviceSwitchLog)
    {
        FString Timecode = FormatNormalizedTimecode(Switch.Key);
        FString Row = FString::Printf(TEXT("%.6f,%s,%s"), 
            Switch.Key, *Timecode, *Switch.Value.ToString());
        SwitchRows.Add(Row);
    }
    
    const FString Content = FString::Join(SwitchRows, TEXT("\n"));
    if (FFileHelper::SaveStringToFile(Content, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("Active Phone Writer: Exported %d device switches to %s"),
            DeviceSwitchLog.Num(), *FullPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Active Phone Writer: Failed to save switches log to %s"), *FullPath);
        return false;
    }
}

// void UActiveLiveLinkCSVWriter::ClearDeviceRegistry()
// {
//     ULLFDeviceRegistry* DeviceRegistry = ConnMan->GetDeviceRegistry();
//     if (!DeviceRegistry)
//         return;
// 
//     if (DeviceRegistry)
//     {
//         DeviceRegistry->OnDeviceActivated.RemoveDynamic(this, &UActiveLiveLinkCSVWriter::HandleDeviceActivated);
//         DeviceRegistry->OnDeviceDeactivated.RemoveDynamic(this, &UActiveLiveLinkCSVWriter::HandleDeviceDeactivated);
//     }
// 
//     DeviceRegistry = nullptr;
// }
