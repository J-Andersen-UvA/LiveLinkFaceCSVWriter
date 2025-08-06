#include "LiveLinkFaceCSVWriterComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "FMath.h"

ULiveLinkFaceCSVWriterComponent::ULiveLinkFaceCSVWriterComponent()
    : SubjectName(NAME_None)
    , Filename(TEXT("LiveLinkFaceData.csv"))
    , bIsRecording(false)
    , bHeaderWritten(false)
    , LiveLinkClient(nullptr)
{
    // Enable ticking (but start disabled until recording)
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    // Cache the LiveLink client
    LiveLinkClient = FLiveLinkClientReference::GetClient();
    check(LiveLinkClient);
}

void ULiveLinkFaceCSVWriterComponent::BeginPlay()
{
    Super::BeginPlay();
}

void ULiveLinkFaceCSVWriterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsRecording)
    {
        if (!bHeaderWritten)
        {
            if (!InitializeCSVHeader())
            {
                // Failed to write header; stop recording
                StopRecording();
                return;
            }
        }
        CaptureFrame();
    }
}

void ULiveLinkFaceCSVWriterComponent::SetSubjectName(const FName& InSubjectName)
{
    SubjectName = InSubjectName;
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Subject set to %s"), *SubjectName.ToString());
}

void ULiveLinkFaceCSVWriterComponent::SetFilename(const FString& InFilename)
{
    Filename = InFilename;
    if (!Filename.EndsWith(TEXT(".csv")))
    {
        Filename += TEXT(".csv");
    }
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Filename set to %s"), *Filename);
}

bool ULiveLinkFaceCSVWriterComponent::StartRecording()
{
    if (SubjectName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Cannot start – no subject set"));
        return false;
    }

    // Enable TickComponent
    bIsRecording = true;
    bHeaderWritten = false;
    CSVRows.Empty();
    SetComponentTickEnabled(true);

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Started recording subject '%s'"), *SubjectName.ToString());
    return true;
}

void ULiveLinkFaceCSVWriterComponent::StopRecording()
{
    bIsRecording = false;
    SetComponentTickEnabled(false);
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Stopped recording"));
}

bool ULiveLinkFaceCSVWriterComponent::ExportFile()
{
    if (CSVRows.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("LiveLink CSV Writer: No data to export"));
        return false;
    }

    // Save under Saved/LiveLinkExports/
    const FString FullPath = FPaths::ProjectSavedDir() / TEXT("LiveLinkExports") / Filename;
    const FString Dir = FPaths::GetPath(FullPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
    {
        PF.CreateDirectoryTree(*Dir);
    }

    const FString Content = FString::Join(CSVRows, TEXT("\n"));
    if (FFileHelper::SaveStringToFile(Content, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Exported %d rows to %s"),
               CSVRows.Num(), *FullPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Failed to save to %s"), *FullPath);
        return false;
    }
}

bool ULiveLinkFaceCSVWriterComponent::InitializeCSVHeader()
{
    if (SubjectName.IsNone() || !LiveLinkClient)
    {
        return false;
    }

    FLiveLinkSubjectFrameData FrameData;
    if (!LiveLinkClient->EvaluateFrame_AnyThread(
            SubjectName,
            ULiveLinkBasicRole::StaticClass(),
            FrameData))
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Failed to evaluate frame for header"));
        return false;
    }

    // Extract curve names
    CurveNames = FrameData.StaticData.CurveNames;
    const int32 NumCurves = CurveNames.Num();

    // Build header row
    TArray<FString> HeaderCols;
    HeaderCols.Add(TEXT("Timecode"));
    HeaderCols.Add(FString::FromInt(NumCurves));
    for (const FName& N : CurveNames)
    {
        HeaderCols.Add(N.ToString());
    }

    CSVRows.Empty();
    CSVRows.Add(FString::Join(HeaderCols, TEXT(",")));
    bHeaderWritten = true;

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Header initialized (%d curves)"), NumCurves);
    return true;
}

void ULiveLinkFaceCSVWriterComponent::CaptureFrame()
{
    if (!LiveLinkClient || SubjectName.IsNone())
    {
        return;
    }

    FLiveLinkSubjectFrameData FrameData;
    if (!LiveLinkClient->EvaluateFrame_AnyThread(
            SubjectName,
            ULiveLinkBasicRole::StaticClass(),
            FrameData))
    {
        return;
    }

    // If this tick’s values exactly match the last tick, skip writing
    if (LastCurveValues.Num() == FrameData.CurveElements.Num() &&
        LastCurveValues == FrameData.CurveElements)
    {
        return;  // no change, bail out
    }

    TArray<FString> Cols;
    // Timecode
    Cols.Add(FormatTimecode(FrameData.MetaData.SceneTime));
    // Count
    Cols.Add(FString::FromInt(FrameData.CurveElements.Num()));
    // Values
    for (double V : FrameData.CurveElements)
    {
        Cols.Add(FString::SanitizeFloat(V));
    }

    CSVRows.Add(FString::Join(Cols, TEXT(",")));
    LastCurveValues = FrameData.CurveElements;
}

FString ULiveLinkFaceCSVWriterComponent::FormatTimecode(const FQualifiedFrameTime& QT) const
{
    const auto& TC = QT.Timecode;
    int32 Millis = FMath::RoundToInt(TC.SubFrame * 1000.0f);
    return FString::Printf(TEXT("%02d:%02d:%02d:%02d.%03d"),
                           TC.Hours, TC.Minutes, TC.Seconds, TC.Frames, Millis);
}
