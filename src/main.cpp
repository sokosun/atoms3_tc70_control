#include "M5AtomS3.h"
#include "MadgwickAHRS.h"
#include <WiFi.h>
#include "TC70Control.h"

#define GPIO_BUTTON 41

// Please modify
const char* ssid     = "SSID";
const char* password = "PASSWORD";
const IPAddress tc70_ipaddr(192,168,1,63);
const String tc70_username("tc70_username");
const String tc70_password("tc70_password");

TC70Control tc70control(tc70_ipaddr, tc70_username, tc70_password);
TC70Control::UriList g_uris;
TC70Control::Profile g_prof;
TC70Control::PTSpace g_ptspace;

Madgwick madgwick;


// Posture holds roll, pitch and yaw of the AtomS3
struct Posture {
  float roll_deg;
  float pitch_deg;
  float yaw_deg;

private:
  float normalize(float deg){
    while(deg >= 180){
      deg -= 360;
    }
    while(deg < -180){
      deg += 360;
    }
    return deg;
  }

public:
  Posture(float roll_deg_in = 0, float pitch_deg_in = 0, float yaw_deg_in = 0){
    roll_deg  = normalize(roll_deg_in);
    pitch_deg = normalize(pitch_deg_in);
    yaw_deg   = normalize(yaw_deg_in);
  }

  const Posture operator-(const Posture& rhs) const{
    return Posture(this->roll_deg - rhs.roll_deg, this->pitch_deg - rhs.pitch_deg, this->yaw_deg - rhs.yaw_deg);
  }

  bool operator==(const Posture& rhs) const {
    return this->roll_deg == rhs.roll_deg && this->pitch_deg == rhs.pitch_deg && this->yaw_deg == rhs.yaw_deg;
  }

  bool operator!=(const Posture& rhs) const {
    return !(*this != rhs);
  }
};

Posture g_offset; // Hold yaw on press the button


volatile bool g_irq0 = false;
void setIRQ0() {
  g_irq0 = true;
}

void setup() {
  M5.begin(false, true, true, false);
  M5.IMU.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  USBSerial.printf("WiFi connected\r\n");

  madgwick.begin(20);

  pinMode(GPIO_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GPIO_BUTTON), setIRQ0, FALLING);

  // ONVIF-PTZ requires time for each packet.
  const long nine_hours = 9 * 3600; // sec.
  configTime(nine_hours, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
}

/// return true if succeeded
bool initTC70() {
  auto capabilities = tc70control.GetCapabilities();
  if(capabilities.isEmpty()){ return false; }
  g_uris = tc70control.ExtractUris(capabilities);

  auto profiles = tc70control.GetProfiles(g_uris.media);
  if(profiles.isEmpty()){ return false; }
  g_prof = tc70control.ExtractFirstProfile(profiles);

  auto configuration_options = tc70control.GetConfigurationOptions(g_uris.ptz, g_prof.ptztoken);
  if(configuration_options.isEmpty()){ return false; }
  g_ptspace = tc70control.ExtractAbsolutePTSpace(configuration_options);

  auto status = tc70control.GetStatus(g_uris.ptz, g_prof.proftoken);
  if(status.isEmpty()){ return false; }
  auto pos = tc70control.ExtractAbsolutePosition(status);

  USBSerial.printf("Media URI:   %s\r\n", g_uris.media);
  USBSerial.printf("Events URI:  %s\r\n", g_uris.events);
  USBSerial.printf("PTZ URI:     %s\r\n", g_uris.ptz);
  USBSerial.printf("Profile Token: %s\r\n", g_prof.proftoken);
  USBSerial.printf("PTZ Token:   %s\r\n", g_prof.ptztoken);
  USBSerial.printf("Pan Space:   %.2f to %.2f\r\n", g_ptspace.PanMin,   g_ptspace.PanMax);
  USBSerial.printf("Tilt Space:  %.2f to %.2f\r\n", g_ptspace.TiltMin,  g_ptspace.TiltMax);
  USBSerial.printf("Speed Limit: %.2f to %.2f\r\n", g_ptspace.SpeedMin, g_ptspace.SpeedMax);
  USBSerial.printf("Current Position: (Pan, Tilt) = (%f, %f)\r\n", pos.pan, pos.tilt);
  return true;
}

// Process for madgwick filter
Posture updatePosture(){
  static unsigned long prev = 0;
  static Posture latest;

  unsigned long curr = millis();
  if(curr - prev < 50){ // 20Hz
    return latest;
  }
  prev = curr;

  float ax, ay, az, gx, gy, gz;
  M5.IMU.getAccel(&ax, &ay, &az);
  M5.IMU.getGyro(&gx, &gy, &gz);

  // Rotate coordinates because AtomS3 connects to a smartphone in landscape mode
  madgwick.updateIMU(gy, gz, gx, ay, az, ax);
  latest = Posture(madgwick.getRoll(), madgwick.getPitch(), madgwick.getYaw());
  return latest;
}


float getRotationValue(float angle_deg, float range_deg, float range_max, float range_min){
  auto rotation_value = angle_deg / range_deg * (range_max - range_min);
  return rotation_value > range_max ? range_max : rotation_value < range_min ? range_min : rotation_value;
}

// Process for TC70 control
void updateTC70(Posture posture){
  static unsigned long prev = 0;
  unsigned long curr = millis();
  if(curr - prev < 100){ // 10Hz
    return;
  }
  prev = curr;

  auto tmp = posture - g_offset;
  auto pan_rotation  = getRotationValue(- tmp.yaw_deg,  TC70Control::PanRange_deg,  g_ptspace.PanMax,  g_ptspace.PanMin);
  auto tilt_rotation = getRotationValue(tmp.roll_deg, TC70Control::TiltRange_deg, g_ptspace.TiltMax, g_ptspace.TiltMin);

#if 0 // dry run
  USBSerial.printf("%f, %f\r\n", pan_rotation, tilt_rotation);
#else
  tc70control.AbsoluteMove(g_uris.ptz, g_prof.proftoken, pan_rotation, tilt_rotation);
#endif

}

void loop(){
  static bool initialized = false;

  auto posture = updatePosture();

  if(initialized){
    updateTC70(posture);
  }

  if(!g_irq0){
    return;
  }

  if(!initialized){
    initialized = initTC70();
  }

  g_offset = posture;
  g_irq0 = false;
}
