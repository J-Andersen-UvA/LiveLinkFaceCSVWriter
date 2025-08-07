#include "LiveLinkFaceCSVWriterComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"
#include "LiveLinkClientReference.h"
#include "ILiveLinkClient.h"
//#include "Roles/LiveLinkFaceRole.h"
#include "Roles/LiveLinkBasicRole.h"


ULiveLinkFaceCSVWriterComponent::ULiveLinkFaceCSVWriterComponent()
    : SubjectName(NAME_None)
    , Filename(TEXT("LiveLinkFaceData.csv"))
    , ExportFolder(FPaths::ProjectSavedDir() / TEXT("LiveLinkExports"))
    , bIsRecording(false)
    , bHeaderWritten(false)
    , LiveLinkClient(nullptr)
{
    // --- enable ticking ---
    PrimaryComponentTick.bCanEverTick = true;   // allow this component to ever tick
    PrimaryComponentTick.bStartWithTickEnabled = false;  // only start ticking when StartRecording() calls SetComponentTickEnabled(true)

    // --- this lives on the component itself ---
    bTickInEditor = true;   // run TickComponent() even when not in PIE
    bAutoActivate = false;
}

void ULiveLinkFaceCSVWriterComponent::BeginPlay()
{
    Super::BeginPlay();

    // We'll get the client dynamically when needed
    LiveLinkClient = nullptr;
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

void ULiveLinkFaceCSVWriterComponent::SetSaveFolder(const FString& InFolderPath)
{
    if (FPaths::IsRelative(InFolderPath))
    {
        ExportFolder = FPaths::ProjectSavedDir() / InFolderPath;
    }
    else
    {
        ExportFolder = InFolderPath;
    }
    
    // Ensure it doesnt end with a slash
    ExportFolder = FPaths::GetPath(ExportFolder / TEXT(""));
    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Export folder set to %s"), *ExportFolder);
}

FString ULiveLinkFaceCSVWriterComponent::GetSaveFolder() const
{
    return ExportFolder;
}

bool ULiveLinkFaceCSVWriterComponent::StartRecording()
{
    if (SubjectName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Cannot start – no subject set"));
        return false;
    }

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

    //const FString FullPath = FPaths::ProjectSavedDir() / TEXT("LiveLinkExports") / Filename;
    const FString FullPath = ExportFolder / Filename;
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

ILiveLinkClient* ULiveLinkFaceCSVWriterComponent::GetLiveLinkClient() const
{
    // Use the static LiveLink client reference approach
    FLiveLinkClientReference ClientRef;
    return ClientRef.GetClient();
}

bool ULiveLinkFaceCSVWriterComponent::InitializeCSVHeader()
{
    if (SubjectName.IsNone())
    {
        return false;
    }

    ILiveLinkClient* Client = GetLiveLinkClient();
    if (!Client)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Could not get LiveLink client"));
        return false;
    }

    // Get the list of subjects to verify our subject exists
    TArray<FLiveLinkSubjectKey> Subjects = Client->GetSubjects(true, true);
    bool bSubjectFound = false;
    for (const FLiveLinkSubjectKey& Subject : Subjects)
    {
        if (Subject.SubjectName == SubjectName)
        {
            bSubjectFound = true;
            break;
        }
    }

    if (!bSubjectFound)
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Subject '%s' not found"), *SubjectName.ToString());
        return false;
    }

    FLiveLinkSubjectFrameData FrameData;
    //if (!Client->EvaluateFrame_AnyThread(SubjectName, ULiveLinkFaceRole::StaticClass(), FrameData))
    if (!Client->EvaluateFrame_AnyThread(SubjectName, ULiveLinkBasicRole::StaticClass(), FrameData))
    {
        UE_LOG(LogTemp, Error, TEXT("LiveLink CSV Writer: Failed to evaluate frame for header"));
        return false;
    }

    // Try to extract curve names from static data
    if (const FLiveLinkBaseStaticData* BaseStaticData = FrameData.StaticData.Cast<FLiveLinkBaseStaticData>())
    {
        CurveNames = BaseStaticData->PropertyNames;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("LiveLink CSV Writer: Could not get property names, using indices"));
        // Fallback: create generic names
        if (const FLiveLinkBaseFrameData* BaseFrameData = FrameData.FrameData.Cast<FLiveLinkBaseFrameData>())
        {
            CurveNames.Empty();
            for (int32 i = 0; i < BaseFrameData->PropertyValues.Num(); ++i)
            {
                CurveNames.Add(*FString::Printf(TEXT("Property_%d"), i));
            }
        }
    }

    const int32 NumCurves = CurveNames.Num();

    TArray<FString> HeaderCols;
    HeaderCols.Add(TEXT("Timecode"));
    HeaderCols.Add(TEXT("NumProperties"));
    for (const FName& N : CurveNames)
    {
        HeaderCols.Add(N.ToString());
    }

    CSVRows.Empty();
    CSVRows.Add(FString::Join(HeaderCols, TEXT(",")));
    bHeaderWritten = true;

    UE_LOG(LogTemp, Log, TEXT("LiveLink CSV Writer: Header initialized (%d properties)"), NumCurves);
    return true;
}

void ULiveLinkFaceCSVWriterComponent::CaptureFrame()
{
    if (SubjectName.IsNone())
    {
        return;
    }

    ILiveLinkClient* Client = GetLiveLinkClient();
    if (!Client)
    {
        return;
    }

    FLiveLinkSubjectFrameData FrameData;
    if (!Client->EvaluateFrame_AnyThread(SubjectName, ULiveLinkBasicRole::StaticClass(), FrameData))
    {
        return;
    }

    TArray<FString> Cols;

    FLiveLinkSubjectFrameData SubjectFrameData;
    if (Client->EvaluateFrame_AnyThread(SubjectName, ULiveLinkBasicRole::StaticClass(), SubjectFrameData))
    {
        const FLiveLinkBaseFrameData* BaseFrame = SubjectFrameData.FrameData.GetBaseData();
        if (BaseFrame)
        {
            // BaseFrame->MetaData.SceneTime is an FQualifiedFrameTime
            Cols.Add(FormatTimecode(BaseFrame->MetaData.SceneTime));
        }
    }

    // Extract property values from the frame data
    if (const FLiveLinkBaseFrameData* BaseFrameData = FrameData.FrameData.Cast<FLiveLinkBaseFrameData>())
    {
        Cols.Add(FString::FromInt(BaseFrameData->PropertyValues.Num()));
        for (float V : BaseFrameData->PropertyValues)
        {
            Cols.Add(FString::Printf(TEXT("%.10f"), V));
        }
    }
    else
    {
        // Fallback - just add the count as 0
        Cols.Add(FString::Printf(TEXT("%.10f"), 0.0f));
    }

    // Join into a CSV line
    const FString NewLine = FString::Join(Cols, TEXT(","));

    // Compare to last row—if identical, skip
    if (CSVRows.Num() > 0 && CSVRows.Last() == NewLine)
    {
        // duplicate frame: do nothing
        return;
    }

    // otherwise append
    CSVRows.Add(NewLine);
}

FString ULiveLinkFaceCSVWriterComponent::FormatTimecode(const FQualifiedFrameTime& QT) const
{
    // Instead of QT.Time.GetTimecode(), use QT.ToTimecode():
    FTimecode TC = QT.ToTimecode();  // :contentReference[oaicite:0]{index=0}

    // Compute milliseconds from the fractional sub-frame:
    int32 Millis = FMath::RoundToInt(QT.Time.GetSubFrame() * 1000.0f / QT.Rate.AsDecimal());

    return FString::Printf(
        TEXT("%02d:%02d:%02d:%02d.%03d"),
        TC.Hours, TC.Minutes, TC.Seconds, TC.Frames, Millis
    );
}
