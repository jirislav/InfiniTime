#pragma once

#include "displayapp/screens/Screen.h"
#include "components/ble/TelephoneBearerService.h"
#include <lvgl/lvgl.h>
#include <cstdint>
#include <string>

namespace Pinetime {
  namespace Controllers {
    class TelephoneBearerService;
  }

  namespace Applications {
    namespace Screens {
      class CallTheGate : public Screen {

      public:
        CallTheGate(Pinetime::Controllers::TelephoneBearerService& telephoneBearerService);
        ~CallTheGate() override;

      private:
        unsigned short lastLabelPosX = 4;
        unsigned short lastLabelPosY = 4;

        lv_obj_t* newLabel();
        lv_obj_t* newTextArea(lv_coord_t);

        Pinetime::Controllers::TelephoneBearerService& tbs;

        const std::wstring numberToCall {L"tel:608069355"};
      };

    }
  }
}
