#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiveLinkClientReference.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkBasicRole.h"
#include "LiveLinkFaceCSVWriterComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LiveLinkFaceCSVWriter_API ULiveLinkFaceCSVWriterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULiveLinkFaceCSVWriterComponent();

    /** Set which LiveLink subject to record */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetSubjectName(const FName& InSubjectName);

    /** Set output CSV filename (will append “.csv” if missing) */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetFilename(const FString& InFilename);

    /** Begin sampling on TickComponent */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool StartRecording();

    /** Stop sampling */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void StopRecording();

    /** Write all captured rows to disk */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    bool ExportFile();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    FName SubjectName;

    FString Filename;

    bool bIsRecording;

    bool bHeaderWritten;
    TArray<FString> CSVRows;
    TArray<FName> CurveNames;
    TArray<double> LastCurveValues;

    ILiveLinkClient* LiveLinkClient;

    bool InitializeCSVHeader();

    /** Sample one frame and append to CSVRows */
    void CaptureFrame();

    /** Format SMPTE timecode + subframe as “HH:MM:SS:FF.mmm” */
    FString FormatTimecode(const FQualifiedFrameTime& QT) const;
};
