#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiveLinkClientReference.h"
#include "LiveLinkTypes.h"
#include "LiveLinkRole.h"
#include "LiveLinkFaceCSVWriterComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIVELINKFACECSVWRITER_API ULiveLinkFaceCSVWriterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULiveLinkFaceCSVWriterComponent();

    /** Set which LiveLink subject to record */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetSubjectName(const FName& InSubjectName);

    /** Set output CSV filename (will append ".csv" if missing) */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetFilename(const FString& InFilename);

    /** Begin sampling on TickComponent */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool StartRecording();

    /** Stop sampling */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void StopRecording();

    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetSaveFolder(const FString& InFolderPath);

    /** Write all captured rows to disk */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool ExportFile();

    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    FString GetSaveFolder() const;

    /** Returns true if SubjectName is known to the LiveLink client */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool IsSubjectAvailable() const;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** LiveLink subject to sample */
    FName SubjectName;

    /** CSV filename (just name.csv, saved under Saved/LiveLinkExports/) */
    FString Filename;
    FString ExportFolder;

    /** Are we actively recording? */
    bool bIsRecording;

    /** Has the header row already been written? */
    bool bHeaderWritten;

    /** In-memory CSV rows (first is header) */
    TArray<FString> CSVRows;

    /** Curve names extracted from static data */
    TArray<FName> CurveNames;

    /** Cached LiveLink client pointer */
    ILiveLinkClient* LiveLinkClient;

    /** Pull static data and write header row */
    bool InitializeCSVHeader();

    /** Sample one frame and append to CSVRows */
    void CaptureFrame();

    /** Format SMPTE timecode + subframe as "HH:MM:SS:FF.mmm" */
    FString FormatTimecode(const FQualifiedFrameTime& QT) const;

    /** Get LiveLink client without including subsystem header */
    ILiveLinkClient* GetLiveLinkClient() const;
};