// This class controls TP-Link TC70 network camera by ONVIF - PTZ protocol.
// This class transmits XML formatted packets to/from a camera through HTTP POST method.
//
// FYI: TP-Link TC70 Range of Motion
// Pan 360 deg.
// Tilt 114 deg.
//
// Notes:
// This class doesn't work with generic ONVIF cameras because xml parser ignores XML namespaces.
//
// Usage:
// 1. Create an instance
//   TC70Control tc70control(tc70_ipaddr, tc70_username, tc70_password);
// 2. Collect Information
//   auto capabilities = tc70control.GetCapabilities();
//   auto uris         = tc70control.ExtractUris(capabilities);
//   auto profiles     = tc70control.GetProfiles(uris.media);
//   auto profile      = tc70control.ExtractFirstProfile(profiles);
//   auto conf_options = tc70control.GetConfigurationOptions(uris.ptz, profile.ptztoken);
//   auto ptspace      = tc70control.ExtractAbsolutePTSpace(conf_options);
// 3. (Optional) Get Current Position
//   auto status       = tc70control.GetStatus(uris.ptz, profile.proftoken);
//   auto current      = tc70control.ExtractAbsolutePosition(status);
// 4. Move
//   auto pan          = - pan_deg / TC70Control::PanRange_deg * (ptspace.PanMax - ptspace.PanMin);
//   pan               = pan > ptspace.PanMax ? ptspace.PanMax ? pan < ptspace.PanMin ? ptspace.PanMin : pan;
//   auto tilt         = tilt_deg / TC70Control::TiltRange_deg * (ptspace.TiltMax - ptspace.TiltMin);
//   tilt              = tilt > ptspace.TiltMax ? ptspace.TiltMax ? tilt < ptspace.TiltMin ? ptspace.TiltMin : tilt;
//   auto response     = tc70control.AbsoluteMove(uris.ptz, profile.proftoken, pan, tilt);

#include <Arduino.h>

class TC70Control {
public:
  static constexpr float PanRange_deg = 360.0f;
  static constexpr float TiltRange_deg = 114.0f;

  static constexpr uint16_t ONVIF_PORT = 2020;
  static constexpr int SHA1_LENGTH = 20; // SHA-1 must return 20 bytes result.
  static constexpr int NONCE_LENGTH = 16;

  struct PTSpace {
    float PanMin   = 0;
    float PanMax   = 0;
    float TiltMin  = 0;
    float TiltMax  = 0;
    float SpeedMin = 0;
    float SpeedMax = 0;
  };

  struct PTPosition {
    float pan = 0;
    float tilt = 0;

    PTPosition(float pan_in = 0, float tilt_in = 0){
      pan = pan_in;
      tilt = tilt_in;
    }
  };

  struct Profile {
    String proftoken;
    String ptztoken;
  };

  struct UriList {
    String media;
    String ptz;
    String events;
  };
  
  TC70Control() = delete;
  TC70Control(IPAddress tc70, String username, String password);
  ~TC70Control();

private:
  struct SecurityParameters {
    String  created; // ISO-8601 formatted time
    uint8_t nonce[NONCE_LENGTH];
    uint8_t password_digest[SHA1_LENGTH];
  };
  SecurityParameters GenerateSecurityParameters(String plain_password);

  String Request(String uri, String payload);

  String PackSoapEnvelope(const String & header, const String & body);
  String PackWebServiceSecurity(String username, String password);
  String PackGetCapabitlities();
  String PackGetProfiles();
  String PackGetConfigurationOptions(const String & ptztoken);
  String PackGetStatus(const String & proftoken);
  String PackAbsoluteMove(const String & proftoken, float x, float y, float vx, float vy);

public:
  // Response of GetCapabilities contains URIs for each service
  String GetCapabilities(const String & uri = "onvif/device_service");

  // Profile contains PTZConfiguration
  String GetProfiles(const String & uri_media);

  // Response of GetConfigurationOptions contains PTZ spaces.
  String GetConfigurationOptions(const String & uri_ptz, const String & ptztoken);

  // Response of GetStatus contains current position.
  String GetStatus(const String & uri_ptz, const String & proftoken);

  String AbsoluteMove(const String & uri_ptz, const String & proftoken, float pan, float tilt, float vx = 1.0, float vy = 1.0);

// Extract functions should be static
  UriList ExtractUris(const String & capabilities);
  Profile ExtractFirstProfile(const String & profiles);
  PTSpace ExtractAbsolutePTSpace(const String & configuration_options);
  PTPosition ExtractAbsolutePosition(const String & status);

private:
  IPAddress m_tc70;
  String m_username;
  String m_password;
};

