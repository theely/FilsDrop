#include <FlashStorage.h>

String ssid = SECRET_SSID;    // Network SSID (name)
String pass = SECRET_PASS;    // Network password (use for WPA, or use as key for WEP)

typedef struct
{
  boolean valid;
  char ssid[100];
  char pass[100];
} Configuration;

FlashStorage(flash_store, Configuration);

Configuration config;

int init_config(){
  config = flash_store.read();
  
  if (config.valid == false) {
      ssid.toCharArray(config.ssid, 100);
      pass.toCharArray(config.pass, 100);
      config.valid = true;
      flash_store.write(config);
  }
  return 0;
}

int write_config(){
      flash_store.write(config);
}
