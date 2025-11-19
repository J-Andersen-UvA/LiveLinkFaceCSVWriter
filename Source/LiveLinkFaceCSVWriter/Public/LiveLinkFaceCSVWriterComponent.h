#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiveLinkClientReference.h"
#include "LiveLinkTypes.h"
#include "LiveLinkRole.h"
#include "LiveLinkFaceCSVWriterComponent.generated.h"

// Forward declarations
class ILiveLinkClient;
class ULiveLinkFaceCSVWriterManager;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIVELINKFACECSVWRITER_API ULiveLinkFaceCSVWriterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULiveLinkFaceCSVWriterComponent();

    /** Set which LiveLink subject to record */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetSubjectName(const FName& InSubjectName);

    /** Get the current subject name */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    FName GetSubjectName() const { return SubjectName; }

    /** Set output CSV filename (will append ".csv" if missing) */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetFilename(const FString& InFilename);

    /** Get the current filename */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    FString GetFilename() const { return Filename; }

    /** Begin sampling on TickComponent */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool StartRecording();

    /** Stop sampling */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void StopRecording();

    /** Check if currently recording */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool IsRecording() const { return bIsRecording; }

    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetSaveFolder(const FString& InFolderPath);

    /** Write all captured rows to disk */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    virtual bool ExportFile();

    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    FString GetSaveFolder() const;

    /** Returns true if SubjectName is known to the LiveLink client */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool IsSubjectAvailable() const;

    /** Get the number of rows captured */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    int32 GetRowCount() const { return CSVRows.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

                               
    /** Pull static data and write header row */
    virtual bool InitializeCSVHeader();

    /** Sample one frame and append to CSVRows */
    virtual void CaptureFrame();

    /** Format SMPTE timecode + subframe as "HH:MM:SS:FF.mmm" */
    FString FormatTimecode(const FQualifiedFrameTime& QT) const;

    /** Get LiveLink client without including subsystem header */
    ILiveLinkClient* GetLiveLinkClient() const;

protected:
    /** LiveLink subject to sample */
    UPROPERTY(BlueprintReadOnly, Category="LiveLink CSV Writer")
    FName SubjectName;

    /** CSV filename (just name.csv, saved under Saved/LiveLinkExports/) */
    UPROPERTY(BlueprintReadOnly, Category="LiveLink CSV Writer")
    FString Filename;

    /** Export folder path */
    UPROPERTY(BlueprintReadOnly, Category="LiveLink CSV Writer")
    FString ExportFolder;

    /** Are we actively recording? */
    UPROPERTY(BlueprintReadOnly, Category="LiveLink CSV Writer")
    bool bIsRecording;

    /** Has the header row already been written? */
    bool bHeaderWritten;

    /** In-memory CSV rows (first is header) */
    TArray<FString> CSVRows;

    /** Curve names extracted from static data */
    TArray<FName> CurveNames;

    /** Cached LiveLink client pointer */
    ILiveLinkClient* LiveLinkClient;

    /** Manager that owns this writer (optional) */
    UPROPERTY()
    ULiveLinkFaceCSVWriterManager* OwnerManager;

    friend class ULiveLinkFaceCSVWriterManager;
};
