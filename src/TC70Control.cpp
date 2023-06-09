#include <regex>
#include <string>
#include "TC70Control.h"
#include "HTTPClient.h"
#include "tinyxml2.h"
#include "libb64/cencode.h"
#include "libb64/cdecode.h"

namespace {
const String XMLDeclaration(R"(<?xml version="1.0" encoding="UTF-8"?>)");

uint8_t * GenerateNonce(){
  static uint8_t nonce[TC70Control::NONCE_LENGTH];
  for(int i=0;i<TC70Control::NONCE_LENGTH;i++){
    nonce[i] = (uint8_t)random();
  }
  return nonce;
}

// obuf must have 20 bytes space.
bool calcSHA1(uint8_t * ibuf, unsigned int ilen, uint8_t * obuf){
  auto olen = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), (const unsigned char *)ibuf, ilen, (unsigned char*)obuf);
  return olen == TC70Control::SHA1_LENGTH;
}

} // anonymous namespace


TC70Control::TC70Control(IPAddress tc70, String username, String password){
  m_tc70 = tc70;
  m_username = username;
  m_password = password;
}
TC70Control::~TC70Control(){}

String TC70Control::Request(String uri, String payload){
  HTTPClient http; 
  http.begin(m_tc70.toString(), ONVIF_PORT, uri);
  http.addHeader("Content-Type", R"(application/soap+xml; charset=utf-8;)");

  auto status = http.POST(payload);
  if(status != HTTP_CODE_OK){
    http.end();
    USBSerial.printf("http status: %d\r\n", status);
    return String();
  }

  auto response = http.getString();
  http.end();
  return response;
}

TC70Control::SecurityParameters TC70Control::GenerateSecurityParameters(String plain_password){
  struct tm current;
  getLocalTime(&current);

  char buf[64];
  strftime(buf, 64, "%Y-%m-%dT%H:%M:%S%z", &current);

  SecurityParameters sp;
  sp.created = String(buf);

  auto nonce = GenerateNonce();
  memcpy(sp.nonce, nonce, NONCE_LENGTH);

  uint8_t nonce_created_password_buf[128];
  uint8_t* dst1 = nonce_created_password_buf;
  uint8_t* dst2 = nonce_created_password_buf + NONCE_LENGTH;
  uint8_t* dst3 = nonce_created_password_buf + NONCE_LENGTH + sp.created.length();
  memcpy(dst1, nonce, NONCE_LENGTH);
  memcpy(dst2, sp.created.c_str(), sp.created.length());
  memcpy(dst3, plain_password.c_str(), plain_password.length());

  int length = NONCE_LENGTH + sp.created.length() + plain_password.length();

  calcSHA1(nonce_created_password_buf, length, sp.password_digest);

  return sp;
}

//------------------------------------------------
// ONVIF Commands

String TC70Control::GetCapabilities(const String & uri){
  auto soap_header = PackWebServiceSecurity(m_username, m_password);
  auto soap_body   = PackGetCapabitlities();
  auto payload     = XMLDeclaration + PackSoapEnvelope(soap_header, soap_body);
  return Request(uri, payload);
}

String TC70Control::GetProfiles(const String & uri){
  auto soap_header = PackWebServiceSecurity(m_username, m_password);
  auto soap_body   = PackGetProfiles();
  auto payload     = XMLDeclaration + PackSoapEnvelope(soap_header, soap_body);
  return Request(uri, payload);
}

String TC70Control::GetConfigurationOptions(const String & uri, const String & token){
  auto soap_header = PackWebServiceSecurity(m_username, m_password);
  auto soap_body   = PackGetConfigurationOptions(token);
  auto payload     = XMLDeclaration + PackSoapEnvelope(soap_header, soap_body);
  return Request(uri, payload);
}

String TC70Control::GetStatus(const String & uri, const String & profile){
  auto soap_header = PackWebServiceSecurity(m_username, m_password);
  auto soap_body   = PackGetStatus(profile);
  auto payload     = XMLDeclaration + PackSoapEnvelope(soap_header, soap_body);
  return Request(uri, payload);
}

String TC70Control::AbsoluteMove(const String & uri, const String & profile, float pan, float tilt, float vx, float vy){
  auto soap_header = PackWebServiceSecurity(m_username, m_password);
  auto soap_body   = PackAbsoluteMove(profile, pan, tilt, vx, vy);
  auto payload     = XMLDeclaration + PackSoapEnvelope(soap_header, soap_body);
  return Request(uri, payload);
}

//------------------------------------------------
// Pack functions

String TC70Control::PackSoapEnvelope(const String & header, const String & body){
  return
  String(R"(<soapenv:Envelope xmlns:soapenv="http://www.w3.org/2003/05/soap-envelope">)") +
  String(  R"(<soapenv:Header>)")  +
             header                +
  String(  R"(</soapenv:Header>)") +
  String(  R"(<soapenv:Body>)")    +
             body                  +
  String(  R"(</soapenv:Body>)")   +
  String(R"(</soapenv:Envelope>)");
}

String TC70Control::PackWebServiceSecurity(String username, String password){
  auto sp = GenerateSecurityParameters(password);

  char nonce_b64[64];
  auto nonce_b64_len = base64_encode_chars((const char *)sp.nonce, NONCE_LENGTH, nonce_b64);
  char password_digest_b64[64];
  auto password_digest_b64_len = base64_encode_chars((const char *)sp.password_digest, SHA1_LENGTH, password_digest_b64);

  return
  String(R"(<wss:Security xmlns:wss="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">)") +
  String(  R"(<wss:UsernameToken>)" ) +
  String(    R"(<wss:Username>)"    ) +
               username               +
  String(    R"(</wss:Username>)"   ) +
  String(    R"(<wss:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">)") +
  String(      password_digest_b64  ) +
  String(    R"(</wss:Password>)"   ) +
  String(    R"(<wss:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">)") +
  String(      nonce_b64            ) +
  String(    R"(</wss:Nonce>)"      ) +
  String(    R"(<wsu:Created xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">)") +
               sp.created             +
  String(    R"(</wsu:Created>)"    ) +
  String(  R"(</wss:UsernameToken>)") +
  String(R"(</wss:Security>)"       );
}

String TC70Control::PackGetCapabitlities(){
  return
  String(R"(<GetCapabilities xmlns="http://www.onvif.org/ver10/device/wsdl">)") +
  String(  R"(<Category>)" ) +
  String(    "All"         ) +
  String(  R"(</Category>)") +
  String(R"(</GetCapabilities>)");
}

String TC70Control::PackAbsoluteMove(const String & proftoken, float pan, float tilt, float vx, float vy){
  String position = String(" x=\"") + String(pan)  + String("\" y=\"") + String(tilt)  + String("\" ");
  String velocity = String(" x=\"") + String(vx) + String("\" y=\"") + String(vy) + String("\" ");

  return
  String(R"(<AbsoluteMove xmlns="http://www.onvif.org/ver20/ptz/wsdl">)") +
  String(  R"(<ProfileToken>)" ) +
             proftoken           +
  String(  R"(</ProfileToken>)") +
  String(  R"(<Position>)"     ) +
  String(    R"(<PanTilt xmlns="http://www.onvif.org/ver10/schema" space="http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace")") +
               position          +
  String(    R"(/>)"           ) +
  String(  R"(</Position>)"    ) +
  String(  R"(<Speed>)"        ) +
  String(    R"(<PanTilt xmlns="http://www.onvif.org/ver10/schema" space="http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace")") +
               velocity          +
  String(    R"(/>)"           ) +
  String(  R"(</Speed>)"       ) +
  String(R"(</AbsoluteMove>)"  );
}

String TC70Control::PackGetProfiles(){
  return String(R"(<ns0:GetProfiles xmlns:ns0="http://www.onvif.org/ver10/media/wsdl"/>)");
}

String TC70Control::PackGetConfigurationOptions(const String & ptztoken){
  return
  String(R"(<ns0:GetConfigurationOptions xmlns:ns0="http://www.onvif.org/ver20/ptz/wsdl">)") +
  String(  R"(<ns0:ConfigurationToken>)"    ) +
                ptztoken                      +
  String(  R"(</ns0:ConfigurationToken>)"   ) +
  String(R"(</ns0:GetConfigurationOptions>)");
}

String TC70Control::PackGetStatus(const String & proftoken){
  return
  String(R"(<ns0:GetStatus xmlns:ns0="http://www.onvif.org/ver20/ptz/wsdl">)") +
  String(  R"(<ns0:ProfileToken>)"  ) +
             proftoken                +
  String(  R"(</ns0:ProfileToken>)" ) +
  String(R"(</ns0:GetStatus>)"      );
}

//------------------------------------------------
// Extract functions

TC70Control::UriList TC70Control::ExtractUris(const String & capabilities){
  tinyxml2::XMLDocument doc;
  auto err = doc.Parse(capabilities.c_str());
  if(err != tinyxml2::XML_SUCCESS){
    return UriList();
  }

  UriList uris;

// This code may throw an exception
  {
    auto xaddr =
    doc.FirstChildElement("SOAP-ENV:Envelope")->
        FirstChildElement("SOAP-ENV:Body")->
        FirstChildElement("tds:GetCapabilitiesResponse")->
        FirstChildElement("tds:Capabilities")->
        FirstChildElement("tt:Media")->
        FirstChildElement("tt:XAddr");
    std::string fullpath(xaddr->GetText());
    std::smatch match;
    std::regex_match(fullpath, match, std::regex(R"(http://(.*?)/(.*))"));

    if(match.size() == 3){
      uris.media = String(match[2].str().c_str());
    }
  }
  {
    auto xaddr =
    doc.FirstChildElement("SOAP-ENV:Envelope")->
        FirstChildElement("SOAP-ENV:Body")->
        FirstChildElement("tds:GetCapabilitiesResponse")->
        FirstChildElement("tds:Capabilities")->
        FirstChildElement("tt:Events")->
        FirstChildElement("tt:XAddr");
    std::string fullpath(xaddr->GetText());
    std::smatch match;
    std::regex_match(fullpath, match, std::regex(R"(http://(.*?)/(.*))"));

    if(match.size() == 3){
      uris.events = String(match[2].str().c_str());
    }
  }
  {
    auto xaddr =
    doc.FirstChildElement("SOAP-ENV:Envelope")->
        FirstChildElement("SOAP-ENV:Body")->
        FirstChildElement("tds:GetCapabilitiesResponse")->
        FirstChildElement("tds:Capabilities")->
        FirstChildElement("tt:PTZ")->
        FirstChildElement("tt:XAddr");
    std::string fullpath(xaddr->GetText());
    std::smatch match;
    std::regex_match(fullpath, match, std::regex(R"(http://(.*?)/(.*))"));

    if(match.size() == 3){
      uris.ptz = String(match[2].str().c_str());
    }
  }
  return uris;
}

TC70Control::Profile TC70Control::ExtractFirstProfile(const String & profiles){
  tinyxml2::XMLDocument doc;
  auto err = doc.Parse(profiles.c_str());
  if(err != tinyxml2::XML_SUCCESS){
    return Profile();
  }

// This code may throw an exception
  auto doc_profile =
  doc.FirstChildElement("SOAP-ENV:Envelope")->
      FirstChildElement("SOAP-ENV:Body")->
      FirstChildElement("trt:GetProfilesResponse")->
      FirstChildElement("trt:Profiles");
  Profile prof;
  prof.proftoken = doc_profile->Attribute("token");
  auto ptz = doc_profile->FirstChildElement("tt:PTZConfiguration");
  prof.ptztoken = ptz->Attribute("token");

  return prof;
}

TC70Control::PTSpace TC70Control::ExtractAbsolutePTSpace(const String & configuration_options){
  tinyxml2::XMLDocument doc;
  auto err = doc.Parse(configuration_options.c_str());
  if(err != tinyxml2::XML_SUCCESS){
    return PTSpace();
  }

// This code may throw an exception
  auto doc_space =
  doc.FirstChildElement("SOAP-ENV:Envelope")->
      FirstChildElement("SOAP-ENV:Body")->
      FirstChildElement("tptz:GetConfigurationOptionsResponse")->
      FirstChildElement("tptz:PTZConfigurationOptions")->
      FirstChildElement("tt:Spaces");
  auto abs_space = doc_space->FirstChildElement("tt:AbsolutePanTiltPositionSpace");
  auto spd_space = doc_space->FirstChildElement("tt:PanTiltSpeedSpace");
  PTSpace pt;
  tinyxml2::XMLUtil::ToFloat(abs_space->FirstChildElement("tt:XRange")->FirstChildElement("tt:Min")->GetText(), &pt.PanMin);
  tinyxml2::XMLUtil::ToFloat(abs_space->FirstChildElement("tt:XRange")->FirstChildElement("tt:Max")->GetText(), &pt.PanMax);
  tinyxml2::XMLUtil::ToFloat(abs_space->FirstChildElement("tt:YRange")->FirstChildElement("tt:Min")->GetText(), &pt.TiltMin);
  tinyxml2::XMLUtil::ToFloat(abs_space->FirstChildElement("tt:YRange")->FirstChildElement("tt:Max")->GetText(), &pt.TiltMax);
  tinyxml2::XMLUtil::ToFloat(spd_space->FirstChildElement("tt:XRange")->FirstChildElement("tt:Min")->GetText(), &pt.SpeedMin);
  tinyxml2::XMLUtil::ToFloat(spd_space->FirstChildElement("tt:XRange")->FirstChildElement("tt:Max")->GetText(), &pt.SpeedMax);

  return pt;
}

TC70Control::PTPosition TC70Control::ExtractAbsolutePosition(const String & status){
  tinyxml2::XMLDocument doc;
  auto err = doc.Parse(status.c_str());
  if(err != tinyxml2::XML_SUCCESS){
    return PTPosition();
  }

// This code may throw an exception
  auto doc_pt =
  doc.FirstChildElement("SOAP-ENV:Envelope")->
      FirstChildElement("SOAP-ENV:Body")->
      FirstChildElement("tptz:GetStatusResponse")->
      FirstChildElement("tptz:PTZStatus")->
      FirstChildElement("tt:Position")->
      FirstChildElement("tt:PanTilt");
  PTPosition pt;
  tinyxml2::XMLUtil::ToFloat(doc_pt->Attribute("x"), &pt.pan);
  tinyxml2::XMLUtil::ToFloat(doc_pt->Attribute("y"), &pt.tilt);

  return pt;
}


