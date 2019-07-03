// to build & run: gcc nvml_example.c -lnvidia-ml -o nvml_stat && ./nvml_stat
// /usr/include/nvml.h
#include <stdio.h>
#include <nvml.h>


int check(nvmlReturn_t error, const char *file, const char *fun, int line, const char *call_str) 
{
    if (error != NVML_SUCCESS) {
        printf("[!] %s/%s:%d %s %s\n", file, fun, line, nvmlErrorString(error), call_str);
        return 0;
    };
    return 1;
}
#define CHK(X) check(X, __FILE__, __func__, __LINE__, #X)

// nvmlReturn_t nvmlDeviceClearAccountingPids ( nvmlDevice_t device ) 
// nvmlReturn_t nvmlDeviceSetAccountingMode ( nvmlDevice_t device, nvmlEnableState_t NVML_FEATURE_ENABLED ) 


int get_device_count(){
    unsigned int count = 0;
    // nvmlReturn_t nvmlDeviceGetCount ( unsigned int* deviceCount ) 
    CHK(nvmlDeviceGetCount(&count));
    return (int) count;
}


int main(){
    CHK(nvmlInitWithFlags(NVML_INIT_FLAG_NO_GPUS));

    int count = get_device_count();
    printf("Devices: %d\n", count);


    for(int i = 0; i < count; i++){
        // nvmlReturn_t nvmlDeviceGetHandleByIndex ( unsigned int  index, nvmlDevice_t* device )
        nvmlDevice_t device;
        CHK(nvmlDeviceGetHandleByIndex(i, &device));

        nvmlUtilization_t util;
        CHK(nvmlDeviceGetUtilizationRates(device, &util));
        
        printf("Utilization: (gpu: %d %%) (memory: %d %%) \n", (int) util.gpu, (int) util.memory);
        
    }
    // Processes running compute:
    
    

    // --
    // 
    // nvmlReturn_t nvmlDeviceGetComputeRunningProcesses ( nvmlDevice_t device, unsigned int* infoCount, nvmlProcessInfo_t* infos ) 
    // nvmlReturn_t nvmlDeviceGetDecoderUtilization ( nvmlDevice_t device, unsigned int* utilization, unsigned int* samplingPeriodUs ) 
    // nvmlReturn_t nvmlDeviceGetEncoderUtilization ( nvmlDevice_t device, unsigned int* utilization, unsigned int* samplingPeriodUs )
    // nvmlReturn_t nvmlDeviceGetPowerUsage ( nvmlDevice_t device, unsigned int* power ) 
    // nvmlReturn_t nvmlDeviceGetTemperature ( nvmlDevice_t device, nvmlTemperatureSensors_t sensorType, unsigned int* temp ) 
    // nvmlReturn_t nvmlDeviceGetUtilizationRates ( nvmlDevice_t device, nvmlUtilization_t* utilization ) 


    // --
    CHK(nvmlShutdown());
    return 0;
}
