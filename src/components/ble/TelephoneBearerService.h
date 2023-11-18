#pragma once

#include <string>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_uuid.h>
#undef max
#undef min

namespace Pinetime {
  namespace System {
    class SystemTask;
  }

  namespace Controllers {
    class NimbleController;
    class NotificationManager;

    class TelephoneBearerService {
    public:
      TelephoneBearerService(NimbleController& nimble, Pinetime::System::SystemTask& systemTask, NotificationManager& notificationManager);

      void Init();

      int OnTelephoneServiceRequested(uint16_t attributeHandle, ble_gatt_access_ctxt* context);

      int callNumber(const std::wstring* number);
      int terminateOriginatedCall();

      bool initialized = false;
      unsigned short serviceRequestCount = 0;
      unsigned short callsInitiated = 0;
      unsigned short callsSuccessful = 0;
      unsigned short terminationsInitiated = 0;
      unsigned short terminationsSuccessful = 0;
      uint16_t lastOctetsSent = 0;

    private:
      enum class ControlPointOpcode : uint8_t {
        Accept = 0x00,
        Terminate = 0x01,
        LocalHold = 0x02,
        LocalRetrieve = 0x03,
        Originate = 0x04,
        Join = 0x05
      };

      enum class ControlPointResultCode : uint8_t {
        Success = 0x00,
        OpcodeNotSupported = 0x01,
        OperationNotPossible = 0x02,
        InvalidCallIndex = 0x03,
        StateMismatch = 0x04,
        LackOfResources = 0x05,
        InvalidOutgoingUri = 0x06
      };

      struct LastNotification {
        bool isSet = false;
        ControlPointOpcode opCode;
        uint8_t callIndex;
        ControlPointResultCode resultCode;
      };

      struct LastNotification lastNotification;

      static constexpr uint16_t TelephoneBearerId {0x184B};
      static constexpr uint16_t CallControlPointId {0x2BBE};

      static constexpr ble_uuid16_t TelephoneBearerUuid {.u {.type = BLE_UUID_TYPE_16}, .value = TelephoneBearerId};
      static constexpr ble_uuid16_t CallControlPointUuid {.u {.type = BLE_UUID_TYPE_16}, .value = CallControlPointId};

      struct ble_gatt_chr_def characteristicDefinition[2];
      struct ble_gatt_svc_def serviceDefinition[2];

      uint16_t callControlPointHandle;

      NimbleController& nimble;
      Pinetime::System::SystemTask& systemTask;
      NotificationManager& notificationManager;
    };
  }
}
