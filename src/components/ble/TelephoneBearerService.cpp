#include "components/ble/TelephoneBearerService.h"
#include "components/ble/NimbleController.h"
#include "components/ble/NotificationManager.h"
#include <nrf_log.h>
#include <cstring>
#include "systemtask/SystemTask.h"

using namespace Pinetime::Controllers;

constexpr ble_uuid16_t TelephoneBearerService::TelephoneBearerUuid;
constexpr ble_uuid16_t TelephoneBearerService::CallControlPointUuid;

namespace {
  int TelephoneBearerServiceCallback(uint16_t /*conn_handle*/, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    auto* TelephoneBearerService = static_cast<Pinetime::Controllers::TelephoneBearerService*>(arg);
    return TelephoneBearerService->OnTelephoneServiceRequested(attr_handle, ctxt);
  }
}

TelephoneBearerService::TelephoneBearerService(NimbleController& nimble,
                                               Pinetime::System::SystemTask& systemTask,
                                               NotificationManager& notificationManager)
  : characteristicDefinition {{.uuid = &CallControlPointUuid.u,
                               .access_cb = TelephoneBearerServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &callControlPointHandle},
                              {0}},
    serviceDefinition {
      {/* Device Information Service */
       .type = BLE_GATT_SVC_TYPE_PRIMARY,
       .uuid = &TelephoneBearerUuid.u,
       .characteristics = characteristicDefinition},
      {0},
    },
    nimble {nimble},
    systemTask {systemTask},
    notificationManager {notificationManager} {
}

void TelephoneBearerService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);

  initialized = true;
}

int TelephoneBearerService::OnTelephoneServiceRequested(uint16_t attributeHandle, ble_gatt_access_ctxt* ctxt) {
  ++serviceRequestCount;
  if (attributeHandle == callControlPointHandle) {

    // TODO: We could subscribe for Bearer List Current Calls in order to be able to also terminate a call in progress
    // See
    // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/37126-TBS-html5/out/en/index-en.html#UUID-28fbfa38-5f21-ff5d-fb42-522f26a6abf2
    // And https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/characteristic_uuids.yaml
    NRF_LOG_INFO("TELEPHONE : handle = %d", callControlPointHandle);
    constexpr size_t callControlPointNotificationSize = 3;

    // Ignore notifications not adhering to the standards:
    // +----------------------------+----------------------+-----------------------+
    // | Requested Opcode (1 octet) | Call Index (1 octet) | Result Code (1 octet) |
    // +----------------------------+----------------------+-----------------------+
    const auto packetLen = OS_MBUF_PKTLEN(ctxt->om);
    if (packetLen != callControlPointNotificationSize) {
      return 0;
    }

    // TODO: Do not blindly overwrite potentially useful information of successful originated call ID
    //       - we should probably manage some kind of internal state to keep track of currently active/held calls
    os_mbuf_copydata(ctxt->om, 0, 1, &lastNotification.opCode);
    os_mbuf_copydata(ctxt->om, 1, 1, &lastNotification.callIndex);
    os_mbuf_copydata(ctxt->om, 2, 1, &lastNotification.resultCode);
    lastNotification.isSet = true;

    const int messageSize = 30;
    char buff[messageSize];
    sprintf(buff,
            "CCPN = oc: %d, ci: %d, rc: %d",
            (uint8_t) lastNotification.opCode,
            lastNotification.callIndex,
            (uint8_t) lastNotification.resultCode);

    NotificationManager::Notification notif;
    memcpy(notif.message.data(), buff, messageSize);
    notif.message[messageSize - 1] = '\0';
    notif.size = messageSize;
    notif.category = NotificationManager::Categories::SimpleAlert;

    auto event = Pinetime::System::Messages::OnNewNotification;
    notificationManager.Push(std::move(notif));
    systemTask.PushMessage(event);
  }
  return 0;
}

int TelephoneBearerService::terminateOriginatedCall() {
  if (lastNotification.isSet) {
    if (lastNotification.opCode == ControlPointOpcode::Originate) {
      ++terminationsInitiated;

      uint8_t buf[2] = {(uint8_t) ControlPointOpcode::Terminate, lastNotification.callIndex};

      struct os_mbuf* om = ble_hs_mbuf_from_flat(buf, 2);
      if (om == NULL) {
        NRF_LOG_ERROR("TELEPHONE: failed creating new mbuf to terminate ci: %d", lastNotification.callIndex);
        return 1;
      }

      uint16_t connectionHandle = nimble.connHandle();

      if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
        NRF_LOG_ERROR("TELEPHONE : failed obtaining nimble handle");
        return 1;
      }

      int res = ble_gattc_notify_custom(connectionHandle, callControlPointHandle, om);
      if (res != 0) {
        NRF_LOG_ERROR("TELEPHONE : failed notifying the handle with mbuf '%s'", om);
        return res;
      }

      ++terminationsSuccessful;
      return 0;
    } else {
      NRF_LOG_ERROR("Cannot terminate non-originated call (opcode=%d)", lastNotification.opCode);
      return 1;
    }
  }
  return 0;
}

int TelephoneBearerService::callNumber(const std::wstring* number) {
  ++callsInitiated;

  // From
  // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/37126-TBS-html5/out/en/index-en.html#UUID-0d20aa83-21fa-c1ec-1ec9-675410a23c55
  // First octet is the OPCODE
  // In case of Originate OPCODE, every other octet is UTF-8 string as the URI

  auto opcode = ControlPointOpcode::Originate;
  struct os_mbuf* om = ble_hs_mbuf_from_flat(&opcode, 1);

  if (om == NULL) {
    NRF_LOG_ERROR("TELEPHONE: failed creating new mbuf with '%s'", opcode);
    return 1;
  }

  int res = os_mbuf_append(om, number->c_str(), number->length() * 4);
  if (res != 0) {
    NRF_LOG_ERROR("TELEPHONE : failed appending mbuf with '%s'", number);
    return res;
  }

  uint16_t connectionHandle = nimble.connHandle();

  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    NRF_LOG_ERROR("TELEPHONE : failed obtaining nimble handle");
    return 1;
  }

  res = ble_gattc_notify_custom(connectionHandle, callControlPointHandle, om);
  if (res != 0) {
    NRF_LOG_ERROR("TELEPHONE : failed notifying the handle with mbuf '%s'", om);
    return res;
  }

  lastOctetsSent = om->om_len;
  ++callsSuccessful;
  return 0;
}
