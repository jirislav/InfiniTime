#include "displayapp/screens/CallTheGate.h"
#include "displayapp/DisplayApp.h"

using namespace Pinetime::Applications::Screens;

CallTheGate::CallTheGate(Pinetime::Controllers::TelephoneBearerService& tbs) : tbs(tbs) {
  lv_obj_t* ta = newTextArea(4);
  lv_textarea_add_text(ta, "Originate URI:\n");

  std::wstring::const_iterator it;
  for (it = numberToCall.begin(); it != numberToCall.end(); ++it) {
    auto converted = _lv_txt_encoded_conv_wc(it.base()[0]);

    lv_textarea_add_char(ta, converted);
  }

  // There is always "Text area" appended to the end of the text area, so let's hide it under the bottom
  lv_textarea_add_text(ta, "\n\n\n\n\n\n\n\n\n");

  lastLabelPosY += 24 * 2;
  lv_label_set_text_fmt(newLabel(), " (%d x 4) bytes", numberToCall.length());
  lv_label_set_text(newLabel(), "-------------------");

  int res = tbs.callNumber(&numberToCall);
  lv_label_set_text_fmt(newLabel(), "Init/Req: %02d / %02d", tbs.initialized ? 1 : 0, tbs.serviceRequestCount);
  lv_label_set_text_fmt(newLabel(), "Res/Octs: %02d / %02d", res, tbs.lastOctetsSent);
  lv_label_set_text_fmt(newLabel(), "Call I/S: %02d / %02d", tbs.callsInitiated, tbs.callsSuccessful);
  lv_label_set_text_fmt(newLabel(), "Term I/S: %02d / %02d", tbs.terminationsInitiated, tbs.terminationsSuccessful);
  lv_label_set_text(newLabel(), "-------------------");
}

lv_obj_t* CallTheGate::newLabel() {
  lv_obj_t* title = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_align(title, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(title, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, lastLabelPosX, lastLabelPosY);
  lastLabelPosY += 24;

  return title;
}

lv_obj_t* CallTheGate::newTextArea(lv_coord_t y) {
  lv_obj_t* ta = lv_textarea_create(lv_scr_act(), nullptr);
  lv_textarea_set_text_align(ta, LV_LABEL_ALIGN_LEFT);
  lv_textarea_set_cursor_hidden(ta, true);
  lv_textarea_set_edge_flash(ta, false);
  lv_textarea_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
  lv_obj_align(ta, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 4, y);

  return ta;
}

CallTheGate::~CallTheGate() {
  tbs.terminateOriginatedCall();
  lv_obj_clean(lv_scr_act());
}
