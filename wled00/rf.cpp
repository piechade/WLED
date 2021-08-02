#include "wled.h"

/*
 * Radio sensor support for generic 21 key RGB remotes
 */

#if defined(WLED_DISABLE_RADIO)
void handleRF() {}
#else

RCSwitch *mySwitch;

unsigned long result;

unsigned long rfCheckedTime = 0;

// brightnessSteps: a static array of brightness levels following a geometric
// progression.  Can be generated from the following Python, adjusting the
// arbitrary 4.5 value to taste:
//
// def values(level):
//     while level >= 5:
//         yield int(level)
//         level -= level / 4.5
// result = [v for v in reversed(list(values(255)))]
// print("%d values: %s" % (len(result), result))
//
// It would be hard to maintain repeatable steps if calculating this on the fly.
const byte brightnessSteps[] = {
    5, 7, 9, 12, 16, 20, 26, 34, 43, 56, 72, 93, 119, 154, 198, 255};
const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(uint8_t);

const int effects[] = {
    FX_MODE_STATIC,
    FX_MODE_COLORTWINKLE,
    FX_MODE_RAINBOW_CYCLE,
    FX_MODE_BREATH,
    FX_MODE_RAINBOW};

const size_t numEffects = sizeof(numEffects) / sizeof(uint8_t);

int lastEffect = FX_MODE_STATIC;

// increment `bri` to the next `brightnessSteps` value
void incBrightness()
{
  // dumb incremental search is efficient enough for so few items
  for (uint8_t index = 0; index < numBrightnessSteps; ++index)
  {
    if (brightnessSteps[index] > bri)
    {
      bri = brightnessSteps[index];
      break;
    }
  }
}

// decrement `bri` to the next `brightnessSteps` value
void decBrightness()
{
  // dumb incremental search is efficient enough for so few items
  for (int index = numBrightnessSteps - 1; index >= 0; --index)
  {
    if (brightnessSteps[index] < bri)
    {
      bri = brightnessSteps[index];
      break;
    }
  }
}

// apply preset or fallback to a effect and palette if it doesn't exist
void presetFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID)
{
  if (!applyPreset(presetID, CALL_MODE_BUTTON))
  {
    effectCurrent = effectID;
    effectPalette = paletteID;
  }
}

void changeEffectSpeed(int8_t amount)
{
  Serial.print("changeEffectSpeed: ");
  Serial.println(amount);
  if (effectCurrent != 0)
  {
    int16_t new_val = (int16_t)effectSpeed + amount;
    effectSpeed = (byte)constrain(new_val, 0.1, 255.1);
    Serial.print("effectSpeed: ");
    Serial.println(effectSpeed);
  }
  else
  { // if Effect == "solid Color", change the hue of the primary color
    CRGB fastled_col;
    fastled_col.red = col[0];
    fastled_col.green = col[1];
    fastled_col.blue = col[2];
    CHSV prim_hsv = rgb2hsv_approximate(fastled_col);
    int16_t new_val = (int16_t)prim_hsv.h + amount;
    if (new_val > 255)
      new_val -= 255; // roll-over if  bigger than 255
    if (new_val < 0)
      new_val += 255; // roll-over if smaller than 0
    prim_hsv.h = (byte)new_val;
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    col[0] = fastled_col.red;
    col[1] = fastled_col.green;
    col[2] = fastled_col.blue;
  }
}

void changeEffectIntensity(int8_t amount)
{
  if (effectCurrent != 0)
  {
    int16_t new_val = (int16_t)effectIntensity + amount;
    effectIntensity = (byte)constrain(new_val, 0.1, 255.1);
  }
  else
  { // if Effect == "solid Color", change the saturation of the primary color
    CRGB fastled_col;
    fastled_col.red = col[0];
    fastled_col.green = col[1];
    fastled_col.blue = col[2];
    CHSV prim_hsv = rgb2hsv_approximate(fastled_col);
    int16_t new_val = (int16_t)prim_hsv.s + amount;
    prim_hsv.s = (byte)constrain(new_val, 0.1, 255.1); // constrain to 0-255
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    col[0] = fastled_col.red;
    col[1] = fastled_col.green;
    col[2] = fastled_col.blue;
  }
}

void nextEffect()
{
  Serial.print("lastEffect: ");
  Serial.println(lastEffect);
  Serial.print("numEffects: ");
  Serial.println(numEffects);
  for (uint8_t index = 0; index < numEffects; ++index)
  {
    if (effects[index] == lastEffect)
    {
      uint8_t test = (unsigned)(index + 1);
      Serial.print("index + 1: ");
      Serial.println(test);
      if (test < numEffects)
      {
        lastEffect = effects[index + 1];
        Serial.print("next: ");
      }
      else
      {
        lastEffect = effects[0];
        Serial.print("frist: ");
      }
        Serial.println(lastEffect);
        presetFallback(0, lastEffect, 0);
      break;
    }
  }
}

void decodeRF(unsigned long code)
{
  decodeRF21(code);

  if (nightlightActive && bri == 0)
    nightlightActive = false;
  colorUpdated(CALL_MODE_BUTTON); //for notifier, RF is considered a button input
}

void decodeRF21(unsigned long code)
{
  switch (code)
  {
  case ARILUX_RF_CODE_KEY_ON:
    bri = briLast;
    break;
  case ARILUX_RF_CODE_KEY_TOGGLE:
    nextEffect();
    break;
  case ARILUX_RF_CODE_KEY_BRIGHT_PLUS:
    incBrightness();
    break;
  case ARILUX_RF_CODE_KEY_BRIGHT_MINUS:
    decBrightness();
    break;
  case ARILUX_RF_CODE_KEY_OFF:
    if (bri > 0)
      briLast = bri;
    bri = 0;
    break;
  case ARILUX_RF_CODE_KEY_SPEED_PLUS:
    changeEffectSpeed(16);
    break;
  case ARILUX_RF_CODE_KEY_SPEED_MINUS:
    changeEffectSpeed(-16);
    break;
  case ARILUX_RF_CODE_KEY_MODE_PLUS:
    changeEffectIntensity(16);
    break;
  case ARILUX_RF_CODE_KEY_MODE_MINUS:
    changeEffectIntensity(-16);
    break;
  case ARILUX_RF_CODE_KEY_RED:
    colorFromUint32(COLOR_RED);
    break;
  case ARILUX_RF_CODE_KEY_GREEN:
    colorFromUint32(COLOR_GREEN);
    break;
  case ARILUX_RF_CODE_KEY_BLUE:
    colorFromUint32(COLOR_BLUE);
    break;
  case ARILUX_RF_CODE_KEY_ORANGE:
    colorFromUint32(COLOR_ORANGE);
    break;
  case ARILUX_RF_CODE_KEY_LTGRN:
    colorFromUint32(COLOR_LTGRN);
    break;
  case ARILUX_RF_CODE_KEY_LTBLUE:
    colorFromUint32(COLOR_LTBLUE);
    break;
  case ARILUX_RF_CODE_KEY_AMBER:
    colorFromUint32(COLOR_AMBER);
    break;
  case ARILUX_RF_CODE_KEY_CYAN:
    colorFromUint32(COLOR_CYAN);
    break;
  case ARILUX_RF_CODE_KEY_PURPLE:
    colorFromUint32(COLOR_PURPLE);
    break;
  case ARILUX_RF_CODE_KEY_YELLOW:
    colorFromUint32(COLOR_YELLOW);
    break;
  case ARILUX_RF_CODE_KEY_PINK:
    colorFromUint32(COLOR_PINK);
    break;
  case ARILUX_RF_CODE_KEY_WHITE:
    colorFromUint32(COLOR_WHITE);
    effectCurrent = 0;
    break;
  default:
    return;
  }
}

void initRF()
{
  if (rfEnabled > 0)
  {
    mySwitch = new RCSwitch();
    mySwitch->enableReceive(rfPin);
  }
}

void handleRF()
{
  rfEnabled = 1;
  if (rfEnabled > 0 && millis() - rfCheckedTime > 500)
  {
    rfCheckedTime = millis();
    if (rfEnabled > 0)
    {
      if (mySwitch == NULL)
      {
        initRF();
        return;
      }

      if (mySwitch->available())
      {
        unsigned long data = mySwitch->getReceivedValue();
        unsigned int bits = mySwitch->getReceivedBitlength();
        int protocol = mySwitch->getReceivedProtocol();
        int delay = mySwitch->getReceivedDelay();

        result = data;

        if (result != 0) // only print results if anything is received ( != 0 )
        {
          Serial.print("RF ");
          Serial.print("Received ");
          Serial.print(result);
          Serial.print(" / ");
          Serial.print(bits);
          Serial.print("bit ");
          Serial.print("Protocol: ");
          Serial.println(protocol);
          Serial.print(" / ");
          Serial.print("Delay: ");
          Serial.println(delay);
        }
        decodeRF(result);
        mySwitch->resetAvailable();
      }
    }
    else if (mySwitch != NULL)
    {
      mySwitch->disableReceive();
      delete mySwitch;
      mySwitch = NULL;
    }
  }
}

#endif
