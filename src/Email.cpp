/* =====================================================================
 *  Email.cpp
 * ===================================================================== */
#include "Email.h"
#include "config.h"
#include "Settings.h"
#include "Logger.h"
#include "TimeSync.h"
#include <ESP_Mail_Client.h>

EmailClass Email;
static SMTPSession smtp;

void EmailClass::begin() {
  MailClient.networkReconnect(true);
}

// HTML page shell shared by alarm + recovery messages.
static String wrap(const String& title, const String& accent,
                   const String& bodyHtml) {
  return
    "<div style='font-family:Segoe UI,Arial,sans-serif;max-width:560px;margin:auto;"
    "border:1px solid #e2e8f0;border-radius:10px;overflow:hidden'>"
    "<div style='background:" + accent + ";color:#fff;padding:18px 22px'>"
    "<h2 style='margin:0'>" + title + "</h2></div>"
    "<div style='padding:22px;color:#1e293b;font-size:15px;line-height:1.6'>" +
    bodyHtml +
    "<hr style='border:none;border-top:1px solid #eee;margin:18px 0'>"
    "<p style='color:#94a3b8;font-size:12px;margin:0'>" + Settings.deviceName +
    " &bull; Refrigerator Monitor v" FW_VERSION " &bull; " + TimeSync.isoNow() +
    "</p></div></div>";
}

bool EmailClass::send(const String& subject, const String& htmlBody) {
  if (!Settings.email.enabled || Settings.email.host.isEmpty() ||
      Settings.email.recipient.isEmpty()) {
    LOGW("email", "send skipped — e-mail not configured/enabled");
    return false;
  }

  Session_Config cfg;
  cfg.server.host_name = Settings.email.host.c_str();
  cfg.server.port      = Settings.email.port;
  cfg.login.email      = Settings.email.user.c_str();
  cfg.login.password   = Settings.email.pass.c_str();
  cfg.login.user_domain = "";
  cfg.time.ntp_server   = NTP_SERVER_1 "," NTP_SERVER_2;
  cfg.time.gmt_offset   = 0;

  SMTP_Message msg;
  msg.sender.name  = "Fridge Monitor";
  msg.sender.email = Settings.email.sender.c_str();
  msg.subject      = subject.c_str();
  msg.addRecipient("", Settings.email.recipient.c_str());
  // Optional second recipient — both addresses receive every alert/notice.
  if (!Settings.email.recipient2.isEmpty())
    msg.addRecipient("", Settings.email.recipient2.c_str());
  msg.html.content = htmlBody.c_str();
  msg.html.charSet = "utf-8";
  msg.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&cfg)) {
    LOGE("email", "SMTP connect failed: " + String(smtp.errorReason().c_str()));
    return false;
  }
  bool ok = MailClient.sendMail(&smtp, &msg);
  smtp.closeSession();
  if (ok) LOGI("email", "sent: " + subject);
  else    LOGE("email", "send failed: " + String(smtp.errorReason().c_str()));
  return ok;
}

bool EmailClass::sendAlarm(const String& name, float temp, float threshold,
                           bool reminder) {
  String subj = String(reminder ? "[REMINDER] " : "[ALARM] ") +
                name + " over temperature";
  String body =
    "<p><b>" + name + "</b> has exceeded its alarm threshold.</p>"
    "<table style='border-collapse:collapse;width:100%'>"
    "<tr><td style='padding:6px 0'>Current temperature</td>"
    "<td style='text-align:right;color:#dc2626;font-weight:700'>" +
    String(temp, 1) + " &deg;C</td></tr>"
    "<tr><td style='padding:6px 0'>Alarm threshold</td>"
    "<td style='text-align:right'>" + String(threshold, 1) + " &deg;C</td></tr>"
    "<tr><td style='padding:6px 0'>Time</td>"
    "<td style='text-align:right'>" + TimeSync.isoNow() + "</td></tr></table>"
    "<p style='margin-top:14px'>Please check the unit. Reminder e-mails will "
    "continue every 30 minutes until the temperature returns to normal.</p>";
  return send(subj, wrap(reminder ? "Temperature Alarm (Reminder)"
                                  : "Temperature Alarm", "#dc2626", body));
}

bool EmailClass::sendRecovery(const String& name, float temp, float threshold) {
  String body =
    "<p><b>" + name + "</b> has returned to a normal temperature.</p>"
    "<table style='border-collapse:collapse;width:100%'>"
    "<tr><td style='padding:6px 0'>Current temperature</td>"
    "<td style='text-align:right;color:#16a34a;font-weight:700'>" +
    String(temp, 1) + " &deg;C</td></tr>"
    "<tr><td style='padding:6px 0'>Alarm threshold</td>"
    "<td style='text-align:right'>" + String(threshold, 1) + " &deg;C</td></tr>"
    "<tr><td style='padding:6px 0'>Time</td>"
    "<td style='text-align:right'>" + TimeSync.isoNow() + "</td></tr></table>"
    "<p style='margin-top:14px'>The alarm has been cleared.</p>";
  return send("[RECOVERED] " + name + " back to normal",
              wrap("Temperature Recovered", "#16a34a", body));
}

bool EmailClass::sendAddress(const String& ip, const String& mode) {
  String url = "http://" + ip + "/";
  String body =
    "<p>The monitor is now reachable at a <b>new network address</b>.</p>"
    "<table style='border-collapse:collapse;width:100%'>"
    "<tr><td style='padding:6px 0'>Web address</td>"
    "<td style='text-align:right'><a href='" + url + "'>" + url + "</a></td></tr>"
    "<tr><td style='padding:6px 0'>Also try</td>"
    "<td style='text-align:right'><a href='http://" DEVICE_HOSTNAME ".local/'>"
    "http://" DEVICE_HOSTNAME ".local/</a></td></tr>"
    "<tr><td style='padding:6px 0'>Connection</td>"
    "<td style='text-align:right'>" + mode + "</td></tr>"
    "<tr><td style='padding:6px 0'>Time</td>"
    "<td style='text-align:right'>" + TimeSync.isoNow() + "</td></tr></table>"
    "<p style='margin-top:14px'>Use this address to open the dashboard. This "
    "message is sent once per new address.</p>";
  return send("[ADDRESS] Monitor reachable at " + ip,
              wrap("New Web Address", "#2563eb", body));
}

bool EmailClass::sendTest() {
  return send("[TEST] Refrigerator Monitor",
              wrap("Test E-mail", "#2563eb",
                   "<p>This is a test message. If you received it, SMTP is "
                   "configured correctly.</p>"));
}
