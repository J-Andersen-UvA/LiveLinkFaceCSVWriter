#pragma once
#include "CoreMinimal.h"
#include "LiveLinkFaceCSVWriterComponent.h"

#if WITH_LIVELINKMULTIIPHONE
#include "LLFDeviceRegistry.h"
#endif

#include "ActiveLiveLinkCSVWriter.generated.h"

/**
 * Child class that records from the "active" device, switching subjects dynamically
 * Requires LiveLinkMultiIPhone plugin to function
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIVELINKFACECSVWRITER_API UActiveLiveLinkCSVWriter : public ULiveLinkFaceCSVWriterComponent
{
    GENERATED_BODY()

public:
    UActiveLiveLinkCSVWriter();

    /** Set the device registry to monitor for active device changes */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    void SetDeviceRegistry(ULLFDeviceRegistry* InRegistry);

    /** Get the device registry */
    UFUNCTION(BlueprintCallable, Category="LiveLink CSV Writer")
    ULLFDeviceRegistry* GetDeviceRegistry() const { return DeviceRegistry; }

    bool StartRecording();
    void StopRecording();

    virtual bool ExportFile() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool InitializeCSVHeader() override;
    virtual void CaptureFrame() override;

    /** Export the normalized timecode CSV */
    bool ExportNormalizedFile();

    /** Export the device switches log */
    bool ExportSwitchesLog();
    
    /** Format normalized timecode from elapsed seconds */
    FString FormatNormalizedTimecode(double Seconds) const;

    /** Recording start time (for normalized timecode) */
    double RecordingStartTime;
    
    /** Current recording elapsed time in seconds */
    double CurrentRecordingTime;
    
    /** Separate CSV rows for normalized timecode file */
    TArray<FString> NormalizedCSVRows;
    
    /** Log of device switches: [RecordingTime, DeviceID] */
    TArray<TPair<double, FName>> DeviceSwitchLog;
    
    /** Last known device ID to detect switches */
    FName LastDeviceID;
    
    /** Has the normalized CSV header been written? */
    bool bNormalizedHeaderWritten;

private:
    /** Callback when active device changes in the registry */
    UFUNCTION()
    void OnActiveDeviceChanged(FName DeviceID);

    /** Reference to the device registry */
    UPROPERTY()
    ULLFDeviceRegistry* DeviceRegistry;
};
