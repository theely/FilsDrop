#include <ArduinoHttpClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>

WiFiSSLClient client_ssl;
HttpClient http_client = HttpClient(client_ssl, "filsdrop-default-rtdb.europe-west1.firebasedatabase.app", 443);

const size_t capacity = 4*JSON_ARRAY_SIZE(5) + JSON_OBJECT_SIZE(4) + 180;
DynamicJsonDocument doc(capacity);



typedef struct
{
  unsigned int epoch;
  char * dateTime;
  float vBat;
  int moisture;
  int dry_count;
  String action;
  
} CycleStats;

typedef struct
{
  bool standby;
  int cyle_rest;
  int dry_cicles;
  int moisture_threshold;
  int pump_time;
  int pump_pushes;
  int work_start;
  int work_end;
} Settings;


static int _cyle_rest     = 60;   //1h
static int _dry_cicles    = 8;            //8h (at least 8h of dryines)
static int _moisture_threshold = 900;
static int _pump_time = 60;     //60s
static int _pump_pushes   = 120;          //10min weatering
static int _work_start    = 7;            
static int _work_end      = 19;          

Settings settings = { true, _cyle_rest, _dry_cicles,  _moisture_threshold,_pump_time,_pump_pushes,_work_start,_work_end };



void readSettings() {

  Serial.println("[FILSDROP] Updating settings...");

  http_client.get("/settings.json");
  // read the status code and body of the response
  int statusCode = http_client.responseStatusCode();
  Serial.print("[HTTP] Status code: ");
  Serial.println(statusCode);

  if(statusCode!=200){
    Serial.println("[FILSDROP] Settings update failed!");
    return;
  }

  DeserializationError error = deserializeJson(doc, http_client.responseBody());
  if(error.code()!=DeserializationError::Ok){
    Serial.print("[HTTP] Unable to Parse JSON! error:");
    Serial.println(error.code());
  }else{
    settings.standby = doc["standby"];
    settings.dry_cicles = doc["dryCicles"];
    settings.cyle_rest = doc["restTime"];
    settings.moisture_threshold = doc["moistureThreshold"];
    settings.pump_time = doc["pumpTime"];
    settings.pump_pushes = doc["pumpPushes"];
    settings.work_start = doc["workStartTime"];
    settings.work_end = doc["workEndTime"];

    Serial.print("[HTTP] Settings: ");
    Serial.print("standby: ");
    Serial.print(settings.standby);
    Serial.print(" restTime: ");
    Serial.print(settings.cyle_rest);
    Serial.print(" dryCicles: ");
    Serial.print(settings.dry_cicles);
    Serial.print(" moistureThreshold: ");
    Serial.println(settings.moisture_threshold);
  }

  return;
}


void pushUpdate(CycleStats cycleStatus) {


  Serial.println("[FILSDROP] Sending logs update...");

  char action[60];
  cycleStatus.action.toCharArray(action, sizeof(action));
  
  char body[512];
  snprintf(body, sizeof(body), "{\"dateTime\" : \"%s\",\"epoch\" : \"%d\", \"moisture\" : \"%d\", \"vBat\" : \"%f\", \"DryCount\" : \"%d\", \"Action\" : \"%s\"}", 
                           cycleStatus.dateTime,
                           cycleStatus.epoch,
                           cycleStatus.moisture,
                           cycleStatus.vBat,
                           cycleStatus.dry_count,
                           action
                           );

 Serial.println(body);

  http_client.post("/logs.json", "application/json", body);
  // read the status code and body of the response
  int statusCode = http_client.responseStatusCode();
  String response = http_client.responseBody(); //even if unsuded this is required to reset the client state
  Serial.print("[HTTP] Status code: ");
  Serial.println(statusCode);

}
