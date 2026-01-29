#ifdef XIAO_NRF52

#include <Arduino.h>
#include <Wire.h>

#include "XiaoNrf52Board.h"

#ifdef NRF52_POWER_MANAGEMENT
// Static configuration for power management
// Values set in variant.h defines
const PowerMgtConfig power_config = {
  .lpcomp_ain_channel = PWRMGT_LPCOMP_AIN,
  .lpcomp_refsel = PWRMGT_LPCOMP_REFSEL,
  .voltage_bootlock = PWRMGT_VOLTAGE_BOOTLOCK
};

void XiaoNrf52Board::initiateShutdown(uint8_t reason) {
  bool enable_lpcomp = (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
                        reason == SHUTDOWN_REASON_BOOT_PROTECT);

  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, enable_lpcomp ? LOW : HIGH);

  if (enable_lpcomp) {
    configureVoltageWake(power_config.lpcomp_ain_channel, power_config.lpcomp_refsel);
  }

  enterSystemOff(reason);
}
#endif // NRF52_POWER_MANAGEMENT

void XiaoNrf52Board::begin() {
  NRF52BoardDCDC::begin();

  // Configure battery voltage ADC
  pinMode(PIN_VBAT, INPUT);
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, LOW);  // Enable VBAT divider for reading
  analogReadResolution(12);
  analogReference(AR_INTERNAL_3_0);
  delay(50);  // Allow ADC to settle

#ifdef PIN_USER_BTN
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
#endif

#if defined(PIN_WIRE_SDA) && defined(PIN_WIRE_SCL)
  Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
#endif

  Wire.begin();

#ifdef P_LORA_TX_LED
  pinMode(P_LORA_TX_LED, OUTPUT);
  digitalWrite(P_LORA_TX_LED, HIGH);
#endif

#ifdef NRF52_POWER_MANAGEMENT
  // Boot voltage protection check (may not return if voltage too low)
  checkBootVoltage(&power_config);
#endif

  delay(10);  // Give sx1262 some time to power up
}

uint16_t XiaoNrf52Board::getBattMilliVolts() {
  // https://wiki.seeedstudio.com/XIAO_BLE#q3-what-are-the-considerations-when-using-xiao-nrf52840-sense-for-battery-charging
  // VBAT_ENABLE must be LOW to read battery voltage
  digitalWrite(VBAT_ENABLE, LOW);
  int adcvalue = analogRead(PIN_VBAT);
  return (adcvalue * ADC_MULTIPLIER * AREF_VOLTAGE) / 4.096;
}

float volatile tempCelsius = 0.0f;

void TEMP_IRQHandler(void){

    MESH_DEBUG_PRINTLN("TEMP_IRQ");
    if (NRF_TEMP->EVENTS_DATARDY)
    {
      NRF_TEMP->EVENTS_DATARDY = 0;

      int32_t temp = NRF_TEMP->TEMP;
      MESH_DEBUG_PRINTLN("Raw temp: y", temp);

      float celsius = (float)temp / 4.0f;
      MESH_DEBUG_PRINTLN("C temp: %f", celsius);

      // Stop TEMP peripheral to save power
      NRF_TEMP->TASKS_STOP = 1;

      tempCelsius = celsius;
    }
}

float XiaoNrf52Board::getTemperatureCelsius() {

    MESH_DEBUG_PRINTLN("getTemperatureCelsius");
    /*
    NRF_TEMP->INTENSET |= TEMP_INTENSET_DATARDY_Enabled;
    NVIC_SetPriority(TEMP_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
    MESH_DEBUG_PRINTLN("NVIC_SetPriority");
    NVIC_EnableIRQ(TEMP_IRQn);
    */
   // Clear pending event
    NRF_TEMP->EVENTS_DATARDY = 0;

    // Enable DATARDY interrupt
    NRF_TEMP->INTENSET = TEMP_INTENSET_DATARDY_Msk;

    // Enable TEMP interrupt in NVIC
    NVIC_ClearPendingIRQ(TEMP_IRQn);
    NVIC_SetPriority(TEMP_IRQn, 7);   // lowest priority
    NVIC_EnableIRQ(TEMP_IRQn);

    MESH_DEBUG_PRINTLN("NVIC_EnableIRQ");

    NRF_TEMP->TASKS_START = 1; /** Start the temperature measurement. */
  
    return tempCelsius; // this is the previous measurement
  }

#endif