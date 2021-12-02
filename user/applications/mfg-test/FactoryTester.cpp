#include "application.h"
#include "Particle.h"
#include "check.h"
#include "deviceid_hal.h"

#include "FactoryTester.h"
#include "TesterCommandTypes.h"

#if PLATFORM_ID == PLATFORM_TRON 
    extern "C" {
    #define BOOLEAN AMBD_SDK_BOOLEAN
    #include "rtl8721d.h"
    #undef BOOLEAN
    }
#else
    #define BIT(x)                  (1 << (x))
#endif

#define MOCK_EFUSE 1 // Use our own RAM buffer for efuse instead of wearing out the hardware MTP/OTP

#define SERIAL    Serial1
//#define SERIAL    Serial

// eFuse data lengths
#define SECURE_BOOT_KEY_LENGTH 32
#define FLASH_ENCRYPTION_KEY_LENGTH 16
#define USER_MTP_DATA_LENGTH 32

// MTP data lengths
#define MOBILE_SECRET_LENGTH        15
#define SERIAL_NUMBER_LENGTH        9  // Bottom 6 characters are WIFI MAC, but not stored in MTP to save space
#define HARDWARE_VERSION_LENGTH     4
#define HARDWARE_MODEL_LENGTH       4

// Logical eFuse addresses
#define EFUSE_SYSTEM_CONFIG 0x0E
#define EFUSE_WIFI_MAC      0x11A
#define EFUSE_USER_MTP      0x160  // TODO: Figure out if this really is MTP and if so, how many writes we have

#define EFUSE_WIFI_MAC_LENGTH 6

#define EFUSE_MOBILE_SECRET_OFFSET 0
#define EFUSE_SERIAL_NUMBER_OFFSET (EFUSE_MOBILE_SECRET_OFFSET + MOBILE_SECRET_LENGTH)
#define EFUSE_HARDWARE_VERSION_OFFSET (EFUSE_SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_LENGTH)
#define EFUSE_HARDWARE_MODEL_OFFSET (EFUSE_HARDWARE_VERSION_OFFSET + HARDWARE_VERSION_LENGTH)

// Physical eFuse addresses
#define EFUSE_FLASH_ENCRYPTION_KEY          0x190
#define EFUSE_SECURE_BOOT_KEY               0x1A0
#define EFUSE_PROTECTION_CONFIGURATION_LOW  0x1C0
#define EFUSE_PROTECTION_CONFIGURATION_HIGH 0x1C1

#define EFUSE_FLASH_ENCRYPTION_KEY_LOCK_BITS (~(BIT(5)|BIT(2))) // Clear Bit 2,5 = Read, Write Forbidden
#define EFUSE_SECURE_BOOT_KEY_LOCK_BITS (~(BIT(6))) // Clear Bit 6 = Write Forbidden

#define EFUSE_SUCCESS 1
#define EFUSE_FAILURE 0

// Logical efuse buffer used for BOTH mocking and real efuse read operations
#define LOGICAL_EFUSE_SIZE 1024
static uint8_t logicalEfuseBuffer[LOGICAL_EFUSE_SIZE];

// Physical buffer used ONLY for mocking physical efuse read/writes
#define PHYSICAL_EFUSE_SIZE 512
static uint8_t physicalEfuseBuffer[PHYSICAL_EFUSE_SIZE];

static uint8_t flashEncryptionKey[FLASH_ENCRYPTION_KEY_LENGTH];
static uint8_t flashEncryptionLockBits[1];
static uint8_t secureBootKey[SECURE_BOOT_KEY_LENGTH];
static uint8_t secureBootLockBits[1];
static uint8_t wifiMAC[EFUSE_WIFI_MAC_LENGTH];
static uint8_t mobileSecret[MOBILE_SECRET_LENGTH];
static uint8_t serialNumber[SERIAL_NUMBER_LENGTH];
static uint8_t hardwareVersion[HARDWARE_VERSION_LENGTH];
static uint8_t hardwareModel[HARDWARE_MODEL_LENGTH];

static const String JSON_KEYS[MFG_TEST_END] = {
    "flash_encryption_key",
    "secure_boot_key",
    "wifi_mac",
    "mobile_secret",
    "serial_number",
    "hw_version",
    "hw_model"
};

struct EfuseData {
    const MfgTestKeyType json_key;
    bool isValid;
    const bool isPhysicaleFuse;
    uint8_t * data;
    const uint32_t length;
    const uint32_t address;
};

static EfuseData efuseFields[EFUSE_DATA_MAX] = {
    {MFG_TEST_FLASH_ENCRYPTION_KEY, false, true,  flashEncryptionKey,      FLASH_ENCRYPTION_KEY_LENGTH, EFUSE_FLASH_ENCRYPTION_KEY},
    {MFG_TEST_FLASH_ENCRYPTION_KEY, false, true,  flashEncryptionLockBits, 1,                           EFUSE_PROTECTION_CONFIGURATION_LOW},
    {MFG_TEST_SECURE_BOOT_KEY,      false, true,  secureBootKey,           SECURE_BOOT_KEY_LENGTH,      EFUSE_SECURE_BOOT_KEY},
    {MFG_TEST_SECURE_BOOT_KEY,      false, true,  secureBootLockBits,      1,                           EFUSE_PROTECTION_CONFIGURATION_LOW},
    {MFG_TEST_WIFI_MAC,             false, false, wifiMAC,                 EFUSE_WIFI_MAC_LENGTH,       EFUSE_WIFI_MAC},
    {MFG_TEST_MOBILE_SECRET,        false, false, mobileSecret,            MOBILE_SECRET_LENGTH,        EFUSE_USER_MTP + EFUSE_MOBILE_SECRET_OFFSET},
    {MFG_TEST_SERIAL_NUMBER,        false, false, serialNumber,            SERIAL_NUMBER_LENGTH,        EFUSE_USER_MTP + EFUSE_SERIAL_NUMBER_OFFSET},
    {MFG_TEST_HW_VERSION,           false, false, hardwareVersion,         HARDWARE_VERSION_LENGTH,     EFUSE_USER_MTP + EFUSE_HARDWARE_VERSION_OFFSET},
    {MFG_TEST_HW_MODEL,             false, false, hardwareModel,           HARDWARE_MODEL_LENGTH,       EFUSE_USER_MTP + EFUSE_HARDWARE_MODEL_OFFSET}
};

// Debug helper
static void print_bytes(uint8_t * buffer, int length){
    for(int i = 0; i < length; i++){
        SERIAL.printf("%02X", buffer[i]);
        delay(1);
    }
    SERIAL.println("");
}

// Wrapper for a control request handle
class Request {
public:
    Request() :
            req_(nullptr) {
    }

    ~Request() {
        destroy();
    }

    int init(ctrl_request* req) {
        if (req->request_size > 0) {
            // Parse request
            auto d = JSONValue::parse(req->request_data, req->request_size);
            CHECK_TRUE(d.isObject(), SYSTEM_ERROR_BAD_DATA);
            data_ = std::move(d);
        }
        req_ = req;
        return 0;
    }

    void destroy() {
        data_ = JSONValue();
        if (req_) {
            // Having a pending request at this point is an internal error
            system_ctrl_set_result(req_, SYSTEM_ERROR_INTERNAL, nullptr, nullptr, nullptr);
            req_ = nullptr;
        }
    }

    template<typename EncodeFn>
    int reply(EncodeFn fn) {
        CHECK_TRUE(req_, SYSTEM_ERROR_INVALID_STATE);
        // Calculate the size of the reply data
        JSONBufferWriter writer(nullptr, 0);
        fn(writer);
        const size_t size = writer.dataSize();
        CHECK_TRUE(size > 0, SYSTEM_ERROR_INTERNAL);
        CHECK(system_ctrl_alloc_reply_data(req_, size, nullptr));
        // Serialize the reply
        writer = JSONBufferWriter(req_->reply_data, req_->reply_size);
        fn(writer);
        CHECK_TRUE(writer.dataSize() == size, SYSTEM_ERROR_INTERNAL);
        return 0;
    }

    void done(int result, ctrl_completion_handler_fn fn = nullptr, void* data = nullptr) {
        if (req_) {
            String printResponse = String(req_->reply_data, req_->reply_size);
            SERIAL.printlnf("resp: %d %s", req_->reply_size, printResponse.c_str()); // DEBUG
            system_ctrl_set_result(req_, result, fn, data, nullptr);
            req_ = nullptr;
            destroy();
        }
    }

    const JSONValue& data() const {
        return data_;
    }

private:
    JSONValue data_;
    ctrl_request* req_;
};


// Input strings
const String FactoryTester::IS_READY = "IS_READY";
const String FactoryTester::SET_DATA = "SET_DATA";
const String FactoryTester::BURN_DATA = "BURN_DATA";
const String FactoryTester::VALIDATE_BURNED_DATA = "VALIDATE_BURNED_DATA";
const String FactoryTester::GET_DEVICE_ID = "GET_DEVICE_ID";

// Output strings
const String FactoryTester::PASS = "\"pass\"";
const String FactoryTester::ERRORS = "\"errors\"";

const String FactoryTester::EFUSE_READ_FAILURE = "\"Failed to read burned data\"";
const String FactoryTester::EFUSE_WRITE_FAILURE = "\"Failed to burn data\"";
const String FactoryTester::DATA_DOES_NOT_MATCH = "\"Burned data does not match\"";
const String FactoryTester::NO_VALID_DATA_BUFFERED = "\"No valid data buffered\"";
const String FactoryTester::RSIP_ENABLE_FAILURE = "\"Failed to enable RSIP\"";
const String FactoryTester::ALREADY_PROVISIONED = "\"Already provisioned with same data\"";

const String FactoryTester::FIELD  = "\"field\"";
const String FactoryTester::MESSAGE  = "\"message\"";
const String FactoryTester::CODE = "\"code\"";


void FactoryTester::setup() {   
    memset(logicalEfuseBuffer, 0xFF, sizeof(logicalEfuseBuffer));
    memset(physicalEfuseBuffer, 0xFF, sizeof(physicalEfuseBuffer));
    for(int i = 0; i < EFUSE_DATA_MAX; i++){
        memset(efuseFields[i].data, 0x00, efuseFields[i].length);
    }
    SERIAL.printlnf("Tron Ready!");
}

int FactoryTester::processUSBRequest(ctrl_request* request) {
    memset(usb_buffer, 0x00, sizeof(usb_buffer));
    memcpy(usb_buffer, request->request_data, request->request_size);

    SERIAL.printlnf("USB request size %d type %d request data length: %d ", request->size, request->type, request->request_size);// DEBUG
    SERIAL.printlnf("data: %s", usb_buffer); // DEBUG
    delay(100);// DEBUG

    static const int KEY_COUNT = MFG_TEST_END;
    Vector<int> resultCodes(KEY_COUNT);
    Vector<String> resultStrings(KEY_COUNT);
    this->command_response[0] = 0;

    Request response;
    CHECK(response.init(request));

    bool allPassed = false;
    bool isSetData = false;
    bool isValidateBurnedData = false;
    bool isSetDataAndProvisioned = false;
    bool isGetDeviceId = false;

    // Pick apart the JSON command
    JSONObjectIterator it(response.data());
    while (it.next()) {
        String firstKey(it.name().data());
        JSONValue firstData = it.value();
        JSONType firstType = firstData.type();

        SERIAL.printlnf("%s : %s : %d", firstKey.c_str(), firstData.toString().data(), firstType); // DEBUG

        isSetData = (firstKey == SET_DATA);
        isValidateBurnedData = (firstKey == VALIDATE_BURNED_DATA);
        isSetDataAndProvisioned = isSetData & isProvisioned();

        if(isSetData || isValidateBurnedData) {
            allPassed = true;
            // Invalidate all currently buffered data
            for(int i = 0; i < EFUSE_DATA_MAX; i++){
                efuseFields[i].isValid = false;
                memset(efuseFields[i].data, 0x00, efuseFields[i].length);
            }

            // Process keys in main object
            JSONObjectIterator innerIterator(firstData);
            while (innerIterator.next()) {
                String innerKey(innerIterator.name().data());
                const char * innerData = (const char *)innerIterator.value().toString(); // TODO: non string types?

                SERIAL.printlnf("%s : %s", innerKey.c_str(), innerData); // DEBUG
                delay(10);

                // Parse data from each key
                for (int i = 0; i < KEY_COUNT; i++) {
                    if(innerKey == JSON_KEYS[i]) {
                        int set_data_result = setData((MfgTestKeyType)i, innerData);
                        resultCodes[i] = set_data_result;
                        resultStrings[i] = this->command_response[0] != 0 ? this->command_response : "";

                        allPassed &= (set_data_result == 0);
                    } 
                }
            }
        }
        else if(firstKey == IS_READY){
            allPassed = true;
        }
        else if(firstKey == BURN_DATA){
            allPassed = burnData(resultCodes, resultStrings);
        }
        else if(firstKey == GET_DEVICE_ID){
            isGetDeviceId = true;
            allPassed = getDeviceId();
        }
        
        if(allPassed && (isValidateBurnedData || isSetDataAndProvisioned)) {
            // Compare the burned data against the data we just buffered to RAM
            allPassed = validateData(resultCodes, resultStrings);
        }
    }

    // DEBUG:
    SERIAL.printlnf("allPassed: %d", allPassed); // DEBUG
    for (int i = 0; i < resultCodes.size(); i++){
        if (resultCodes[i] != 0xFF) {
            SERIAL.printlnf("resultCodes: %d %d", i, resultCodes[i]);
            delay(10);
        }
        if (resultStrings[i] != ""){
            SERIAL.printlnf("resultStrings: %d %s", i, resultStrings[i].c_str());
            delay(10);
        }
    }

    // Response for IS_READY, SET_DATA, BURN_DATA, VALIDATE_BURNED_DATA
    auto commandResponse = [allPassed, resultCodes, resultStrings, isSetDataAndProvisioned](JSONWriter& w) {
        w.beginObject();
            w.name(PASS).value(allPassed);

            if(!allPassed){
                w.name(ERRORS).beginArray();
                for(int i = 0; i < resultCodes.size(); i++){
                    if(resultCodes[i] < 0){
                        int errorCode = resultCodes[i];
                        const char * errorMessage = (resultStrings[i] != "" ? resultStrings[i].c_str() : get_system_error_message(errorCode));

                        w.beginObject();
                            w.name(FIELD).value(JSON_KEYS[i]);                            
                            w.name(MESSAGE).value(errorMessage);
                            w.name(CODE).value(errorCode);
                        w.endObject();
                    }    
                }
                w.endArray();
            }
            else if(isSetDataAndProvisioned) {
                w.name(MESSAGE).value(ALREADY_PROVISIONED);
            }
        w.endObject();
    };

    // Separate response for GET_DEVICE_ID... kind of sloppy.
    char * device_id = this->command_response;
    auto getDeviceIdResponse = [device_id](JSONWriter& w){
        w.beginObject();
            w.name(PASS).value(true);
            w.name("device_id").value(device_id);
        w.endObject();
    };

    // Send response.
    int r;
    if(isGetDeviceId) {
        r = CHECK(response.reply(getDeviceIdResponse));
    }
    else {
        r = CHECK(response.reply(commandResponse));
    }

    response.done(r);
    return r;
}

// Validate and buffer provisioning data RAM
int FactoryTester::setData(MfgTestKeyType command, const char * commandData) {
    int result = -1;

    switch (command) {
        case MFG_TEST_FLASH_ENCRYPTION_KEY:
            result = this->setFlashEncryptionKey(commandData);
            break;   
        case MFG_TEST_SECURE_BOOT_KEY:
            result = this->setSecureBootKey(commandData);
            break;   
        case MFG_TEST_SERIAL_NUMBER:
            result = this->setSerialNumber(commandData);
            break;   
        case MFG_TEST_MOBILE_SECRET:
            result = this->setMobileSecret(commandData);
            break;  
        case MFG_TEST_HW_VERSION:
            result = this->setHardwareVersion(commandData);
            break;  
        case MFG_TEST_HW_MODEL:
            result = this->setHardwareModel(commandData);
            break;
        case MFG_TEST_WIFI_MAC:
            result = this->setWifiMac(commandData);
            break;
        default:
        case MFG_TEST_END:
            break;
    }

    return result;
}

static char hex_char(char c) {
    if ('0' <= c && c <= '9') return (unsigned char)(c - '0');
    if ('A' <= c && c <= 'F') return (unsigned char)(c - 'A' + 10);
    if ('a' <= c && c <= 'f') return (unsigned char)(c - 'a' + 10);
    return 0xFF;
}

static int hex_to_bin(const char *s, char *buff, int length) {
    int result;
    if (!s || !buff || length <= 0) {
        return -1;
    }

    for (result = 0; *s; ++result) {
        unsigned char msn = hex_char(*s++);
        if (msn == 0xFF) {
            return -1;
        }
        
        unsigned char lsn = hex_char(*s++);
        if (lsn == 0xFF) {
            return -1;
        }
        
        unsigned char bin = (msn << 4) + lsn;

        if (length-- <= 0) {
            return -1;
        }
        
        *buff++ = bin;
    }
    return result;
}

static int bin_to_hex(char * dest, int dest_length, const uint8_t * source, int source_length) {
    if(source_length * 2 >= dest_length){
        return -1;
    }

    for(int i = 0, j = 0; j < source_length; i += 2, j++){
        snprintf(&dest[i], 3, "%02X", source[j]);
    }
    return 0;
}

#if PLATFORM_ID != PLATFORM_TRON 
    // Fake efuse functions for non tron platforms
    #define L25EOUTVOLTAGE                      7

    unsigned EFUSE_LMAP_READ(uint8_t * buffer){
        return 0;
    }

    unsigned EFUSE_PMAP_READ8(uint32_t CtrlSetting, uint32_t Addr, uint8_t* Data, uint8_t L25OutVoltage){
        return 0;
    }
#endif

int FactoryTester::writeEfuse(bool physical, uint8_t * data, uint32_t length, uint32_t address) {    
    uint32_t eFuseWrite = EFUSE_SUCCESS;
    uint32_t i = 0;

    if (physical) {
        for (i = 0; (i < length) && (eFuseWrite == EFUSE_SUCCESS); i++) {
            #if MOCK_EFUSE
                physicalEfuseBuffer[address + i] &= data[i];
            #else
                //eFuseWrite = EFUSE_PMAP_WRITE8(0, address + i, &data[i], L25EOUTVOLTAGE); // TODO: UNCOMMENT OUT FOR ACTUAL EFUSE WRITES
            #endif
            
            // SERIAL.printlnf("physical eFuse write byte 0x%02X @ 0x%X", data[i], (unsigned int)(address + i)); // Debug
        }
    }
    else {
        #if MOCK_EFUSE
            memcpy(logicalEfuseBuffer + address, data, length);
        #else 
            // eFuseWrite = EFUSE_LMAP_WRITE(address, length, data); // TODO: UNCOMMENT OUT FOR ACTUAL EFUSE WRITES         
        #endif

        // // Debug
        // SERIAL.printlnf("logical eFuse write @ 0x%X", (unsigned int)(address));
        // print_bytes(data, length);
    }

    return (eFuseWrite == EFUSE_SUCCESS) ? 0 : SYSTEM_ERROR_NOT_ALLOWED;
}

int FactoryTester::readEfuse(bool physical, uint8_t * data, uint32_t length, uint32_t address) {    
    uint32_t eFuseRead = EFUSE_SUCCESS;
    uint32_t i = 0;

    if (physical) {
        for (i = 0; (i < length) && (eFuseRead == EFUSE_SUCCESS); i++) {
            #if MOCK_EFUSE
                data[i] = physicalEfuseBuffer[address + i];
            #else
                eFuseRead = EFUSE_PMAP_READ8(0, address + i, &data[i], L25EOUTVOLTAGE);
            #endif
            //SERIAL.printlnf("readEfuse 0x%02X @ 0x%X", data[i], (unsigned int)(address + i));
        }
    }
    else {
        #if MOCK_EFUSE
            memcpy(data, logicalEfuseBuffer + address, length);
        #else
            eFuseRead = EFUSE_LMAP_READ(logicalEfuseBuffer);
            if(eFuseRead == EFUSE_SUCCESS) {
                memcpy(data, logicalEfuseBuffer + address, length);
            }
        #endif
    }

    // 0 = success, 1 = failure
    return !eFuseRead;
}

int FactoryTester::validateCommandData(const char * data, uint8_t * output_bytes, int output_bytes_length) {
    int len = strlen(data);

    // Validate length of data is as expected
    int bytesLength = len / 2;
    if(bytesLength > output_bytes_length) {
        return SYSTEM_ERROR_TOO_LARGE;
    }
    else if (bytesLength < output_bytes_length) {
        return SYSTEM_ERROR_NOT_ENOUGH_DATA;
    }

    // Validate characters in the serial number.
    for (uint8_t i = 0; i < len; i++) {
        if (!isxdigit(data[i])) {
            SERIAL.printlnf("Validate Hex Data FAILED: invalid char value 0x%02X at index %d", data[i], i);
            return SYSTEM_ERROR_BAD_DATA;        
        }
    }
    
    // Convert Hex string to binary data.
    if (hex_to_bin(data, (char *)output_bytes, output_bytes_length) == -1) {
        SERIAL.printlnf("Validate Hex Data FAILED: hex_to_bin failed");
        return SYSTEM_ERROR_BAD_DATA;        
    }

    return 0;
}

int FactoryTester::validateCommandString(const char * data, int expectedLength) {
    int length = strlen(data);

    //SERIAL.printlnf("validate STR length: %d expectedlength %d", length, expectedLength);
    // Validate length
    if (length > expectedLength){
        return SYSTEM_ERROR_TOO_LARGE;
    }
    else if(length < expectedLength) {
        return SYSTEM_ERROR_NOT_ENOUGH_DATA;
    }

    // Validate string contents
    for (int i = 0; i < length; i++){
        if (!isalnum(data[i])) {
            return SYSTEM_ERROR_BAD_DATA;
        }
    }

    return 0;
}

// Return true if any data is written to efuse
bool FactoryTester::isProvisioned() {
    uint8_t burnedEfuseDataBuffer[32];
    int efuseReadResult;

    for(unsigned i = 0; i < EFUSE_DATA_MAX; i++){
        EfuseData data = efuseFields[i];
        efuseReadResult = readEfuse(data.isPhysicaleFuse, burnedEfuseDataBuffer, data.length, data.address);
        if(efuseReadResult != 0){
            // If any efuse data fails to read, it must be locked, and therefore provisioned
            return true;
        }

        for(unsigned i = 0; i < data.length; i++) {
            if(burnedEfuseDataBuffer[i] != 0xFF){ // TODO: How will this handle P2 units with WIFI mac already burned?
                return true;
            }
        }
    }

    return false;
}

bool FactoryTester::burnData(Vector<int> &resultCodes, Vector<String> &resultStrings) {
    bool allBufferedDataValid = true;
    int efuseWriteResult;
    uint8_t userMTPBuffer[USER_MTP_DATA_LENGTH] = {0x00};

    // Check that all buffered data is valid, if not return
    for(int i = 0; i < EFUSE_DATA_MAX; i++){
        EfuseData data = efuseFields[i];
        if(!data.isValid) {
            resultCodes[data.json_key] = -1;
            resultStrings[data.json_key] = NO_VALID_DATA_BUFFERED;
            allBufferedDataValid = false;
        }
    }

    if(!allBufferedDataValid){
        return false;
    }

    // Write all buffered data to appropriate efuse locations (physical and logical)
    for(int i = 0; i < EFUSE_DATA_MAX; i++){
        EfuseData data = efuseFields[i];
        SERIAL.printlnf("burning %d data.address: 0x%lx, data.length %lu", i, data.address, data.length); // debug
        delay(10);

        if(data.isPhysicaleFuse || (data.json_key == MFG_TEST_WIFI_MAC)){
            int efuseWriteResult = writeEfuse(data.isPhysicaleFuse, data.data, data.length, data.address);
            if(efuseWriteResult != 0){
                resultCodes[data.json_key] = efuseWriteResult;
                resultStrings[data.json_key] = EFUSE_WRITE_FAILURE;
                return false;
            }
        }
        // Else buffer logical efuse data
        else {
            memcpy(userMTPBuffer + (data.address - EFUSE_USER_MTP), data.data, data.length);
        }
    }

    // Combine user MTP data fields to be efficient with logical efuse MTP write operations
    // User serial number as standin for whole efuse failure
    efuseWriteResult = writeEfuse(false, userMTPBuffer, USER_MTP_DATA_LENGTH, EFUSE_USER_MTP);
    if(efuseWriteResult != 0){
        resultCodes[MFG_TEST_SERIAL_NUMBER] = efuseWriteResult;
        resultStrings[MFG_TEST_SERIAL_NUMBER] = EFUSE_WRITE_FAILURE;
        return false;
    }

    // Enable RSIP as last step if everything else succeeded
    efuseWriteResult = setFlashEncryption();
    if(efuseWriteResult != 0) {
        resultCodes[MFG_TEST_FLASH_ENCRYPTION_KEY] = -1;
        resultStrings[MFG_TEST_FLASH_ENCRYPTION_KEY] = RSIP_ENABLE_FAILURE;
        return false;   
    }

    return true;
}

// Read data from efuse, compare to ram buffers
// If all data matches, return true, else false
bool FactoryTester::validateData(Vector<int> &resultCodes, Vector<String> &resultStrings) {
    bool allMatch = true;
    uint8_t burnedEfuseDataBuffer[32];
    int efuseReadResult;
    int compareResult;

    // For each efuse item
    for(int i = 0; i < EFUSE_DATA_MAX; i++){
        EfuseData data = efuseFields[i];
        SERIAL.printlnf("validating %d data.address: 0x%lx, data.length %lu", i, data.address, data.length); // debug
        delay(10);

        // Check that all buffered data is valid, if not return    
        if(!data.isValid) {
            resultCodes[data.json_key] = -1;
            resultStrings[data.json_key] = NO_VALID_DATA_BUFFERED;
            allMatch = false;
            continue;
        }

        // Flash encryption key is unreadable after data is burned, no way to explicitly validate it, skip it
        if(i == EFUSE_DATA_FLASH_ENCRYPTION_KEY){
            continue;
        }

        // Read the item into a generic buffer, using efuse data parameters
        efuseReadResult = readEfuse(data.isPhysicaleFuse, burnedEfuseDataBuffer, data.length, data.address);
        if(efuseReadResult != 0){
            resultCodes[data.json_key] = -1;
            resultStrings[data.json_key] = EFUSE_READ_FAILURE;
            allMatch = false;
        }

        // Special case for checking lock bits in configuration byte
        if((i == EFUSE_DATA_FLASH_ENCRYPTION_LOCK_BITS) || (i == EFUSE_DATA_SECURE_BOOT_LOCK_BITS)) {
            compareResult = ((burnedEfuseDataBuffer[0] & data.data[0]) != burnedEfuseDataBuffer[0]);
        }
        else {
            // Compare the default buffer to the ram buffer
            compareResult = memcmp(burnedEfuseDataBuffer, data.data, data.length);    
        }
        
        if(compareResult != 0) {
            resultCodes[data.json_key] = -1;
            resultStrings[data.json_key] = DATA_DOES_NOT_MATCH;  
            allMatch = false;

            print_bytes(burnedEfuseDataBuffer, data.length);//debug
        }
    }

    return allMatch;
}

int FactoryTester::setFlashEncryptionKey (const char * key) {
    EfuseData * flashKeyEfuse = &efuseFields[EFUSE_DATA_FLASH_ENCRYPTION_KEY];
    EfuseData * lockBits = &efuseFields[EFUSE_DATA_FLASH_ENCRYPTION_LOCK_BITS];
    flashKeyEfuse->isValid = false; 
    lockBits->isValid = false;

    int result = validateCommandData(key, flashKeyEfuse->data, flashKeyEfuse->length);
    if(result != 0) {
        return result;
    }

    flashKeyEfuse->isValid = true;
    
    uint8_t configurationByte = 0xFF;
    readEfuse(true, &configurationByte, 1, EFUSE_PROTECTION_CONFIGURATION_LOW);
    lockBits->data[0] = configurationByte & EFUSE_FLASH_ENCRYPTION_KEY_LOCK_BITS;
    lockBits->isValid = true;
    return 0;
}

int FactoryTester::setFlashEncryption(void) {
    uint8_t systemConfigByte = 0;
    readEfuse(false, &systemConfigByte, 1, EFUSE_SYSTEM_CONFIG);
    systemConfigByte |= (BIT(2));

    return this->writeEfuse(false, &systemConfigByte, sizeof(systemConfigByte), EFUSE_SYSTEM_CONFIG);
}

int FactoryTester::setSecureBootKey(const char * key) {
    EfuseData * secureBootKeyEfuse = &efuseFields[EFUSE_DATA_SECURE_BOOT_KEY];
    EfuseData * lockBits = &efuseFields[EFUSE_DATA_SECURE_BOOT_LOCK_BITS];

    secureBootKeyEfuse->isValid = false; 
    lockBits->isValid = false;

    int result = validateCommandData(key, secureBootKeyEfuse->data, secureBootKeyEfuse->length);
    if(result != 0) {
        return result;
    }

    secureBootKeyEfuse->isValid = true;

    uint8_t configurationByte = 0xFF;
    readEfuse(true, &configurationByte, 1, EFUSE_PROTECTION_CONFIGURATION_LOW);
    lockBits->data[0] = configurationByte & EFUSE_SECURE_BOOT_KEY_LOCK_BITS;
    lockBits->isValid = true;
    return 0;
}

int FactoryTester::GET_SECURE_BOOT_KEY(void) {
    uint8_t boot_key[SECURE_BOOT_KEY_LENGTH];
    readEfuse(true, boot_key, sizeof(boot_key), EFUSE_SECURE_BOOT_KEY);

    return bin_to_hex(this->command_response, this->cmd_length, boot_key, sizeof(boot_key));
}

int FactoryTester::ENABLE_SECURE_BOOT(void) {
    return SYSTEM_ERROR_NOT_ALLOWED;

    // // SECURE BOOT WILL BE ENABLED AT A FUTURE DATE. EXAMPLE CODE TO DO SO ONLY
    // uint8_t protectionConfigurationByte = 0;
    // readEfuse(true, &protectionConfigurationByte, 1, EFUSE_PROTECTION_CONFIGURATION_HIGH);

    // protectionConfigurationByte &= ~(BIT(5));

    // // Write enable bit to physical eFuse
    // return this->writeEfuse(true, &protectionConfigurationByte, 1, EFUSE_PROTECTION_CONFIGURATION_HIGH);
}

int FactoryTester::GET_SERIAL_NUMBER(void) {
    char fullSerialNumber[SERIAL_NUMBER_LENGTH + (EFUSE_WIFI_MAC_LENGTH * 2) + 1] = {0};
    readEfuse(false, (uint8_t *)fullSerialNumber, SERIAL_NUMBER_LENGTH, EFUSE_USER_MTP + EFUSE_SERIAL_NUMBER_OFFSET);

    uint8_t mac[EFUSE_WIFI_MAC_LENGTH+1] = {0};
    readEfuse(false, mac, EFUSE_WIFI_MAC_LENGTH, EFUSE_WIFI_MAC);

    for(int i = 0, j = (EFUSE_WIFI_MAC_LENGTH / 2); j < EFUSE_WIFI_MAC_LENGTH; i += 2, j++){
        snprintf(&fullSerialNumber[SERIAL_NUMBER_LENGTH + i], 3, "%02X", mac[j]);
    }

    strcpy(this->command_response, fullSerialNumber);
    return 0;
}

int FactoryTester::setSerialNumber(const char * serial_number) {
    // TODO: Consider taking 15 chars as input and writing to MAC address as well
    // OR: Only requiring 9 chars as input, and wifi mac explicitly sent elsewhere
    EfuseData * serialNumberData = &efuseFields[EFUSE_DATA_SERIAL_NUMBER];
    int result = validateCommandString(serial_number, SERIAL_NUMBER_LENGTH + EFUSE_WIFI_MAC_LENGTH);

    if (result == 0) {
        memcpy(serialNumberData->data, (uint8_t *)serial_number, serialNumberData->length);
    }

    serialNumberData->isValid = (result == 0);
    return result;
}

int FactoryTester::GET_MOBILE_SECRET(void) {
    char mobileSecret[MOBILE_SECRET_LENGTH+1] = {0};
    readEfuse(false, (uint8_t *)mobileSecret, MOBILE_SECRET_LENGTH, EFUSE_USER_MTP + EFUSE_MOBILE_SECRET_OFFSET);
    strcpy(this->command_response, mobileSecret);
    return 0;
}

int FactoryTester::setMobileSecret(const char * mobile_secret) {
    EfuseData * mobileSecretData = &efuseFields[EFUSE_DATA_MOBILE_SECRET];
    int result = validateCommandString(mobile_secret, mobileSecretData->length);

    if (result == 0){
        memcpy(mobileSecretData->data, (uint8_t *)mobile_secret, mobileSecretData->length);
    }

    mobileSecretData->isValid = (result == 0);
    return result;
}

int FactoryTester::GET_HW_VERSION(void) {
    uint8_t hardwareData[HARDWARE_VERSION_LENGTH] = {0};
    readEfuse(false, hardwareData, HARDWARE_VERSION_LENGTH, EFUSE_USER_MTP + EFUSE_HARDWARE_VERSION_OFFSET);

    return bin_to_hex(this->command_response, this->cmd_length, hardwareData, sizeof(hardwareData));
}

int FactoryTester::setHardwareVersion(const char * hardware_version) {
    EfuseData * hardwareVersionData = &efuseFields[EFUSE_DATA_HARDWARE_VERSION];
    int result = validateCommandData(hardware_version, hardwareVersionData->data, hardwareVersionData->length);
    hardwareVersionData->isValid = (result == 0);
    return result;
}

int FactoryTester::GET_HW_MODEL(void) {
    uint8_t hardwareModel[HARDWARE_MODEL_LENGTH] = {0};
    readEfuse(false, hardwareModel, HARDWARE_MODEL_LENGTH, EFUSE_USER_MTP + EFUSE_HARDWARE_MODEL_OFFSET);

    return bin_to_hex(this->command_response, this->cmd_length, hardwareModel, sizeof(hardwareModel));
}

int FactoryTester::setHardwareModel(const char * hardware_model) {
    EfuseData * hardwareModelData = &efuseFields[EFUSE_DATA_HARDWARE_MODEL];
    int result = validateCommandData(hardware_model, hardwareModelData->data, hardwareModelData->length);
    hardwareModelData->isValid = (result == 0);
    return result;
}

int FactoryTester::GET_WIFI_MAC(void) {
    uint8_t wifiMAC[EFUSE_WIFI_MAC_LENGTH] = {0};
    readEfuse(false, wifiMAC, EFUSE_WIFI_MAC_LENGTH, EFUSE_WIFI_MAC);

    return bin_to_hex(this->command_response, this->cmd_length, wifiMAC, sizeof(wifiMAC));
}

int FactoryTester::setWifiMac(const char * wifi_mac) {
    EfuseData * wifimacData = &efuseFields[EFUSE_DATA_WIFI_MAC];
    int result = validateCommandData(wifi_mac, wifimacData->data, wifimacData->length);
    wifimacData->isValid = (result == 0);
    return result;
}

int FactoryTester::getDeviceId(void) {
    uint8_t deviceIdBuffer[HAL_DEVICE_ID_SIZE];
    hal_get_device_id(deviceIdBuffer, sizeof(deviceIdBuffer));
    return bin_to_hex(this->command_response, sizeof(this->command_response), 
                      deviceIdBuffer, sizeof(deviceIdBuffer));
}

int FactoryTester::testCommand(void) {
    for(int i = 0; i < EFUSE_DATA_MAX; i++){
        EfuseData data = efuseFields[i];
        SERIAL.printlnf("i: %d valid: %d", i, data.isValid);
        if(data.isValid){
            print_bytes(data.data, data.length);
        }
        delay(10);
    }

    return 0;
}

char * FactoryTester::get_command_response(void) {
    return command_response;
}

