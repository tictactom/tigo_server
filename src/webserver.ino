void setupWebserver() {
  handleRoot();
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.on("/spiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateFileListHTML());
  });
  handleFileUpload();

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String fileToDelete = request->getParam("file")->value();
      if (SPIFFS.exists(fileToDelete)) {
        if (SPIFFS.remove(fileToDelete)) {
          request->redirect("/spiffs");
        } else {
          request->send(500, "text/plain", "Error deleting file");
        }
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Missing parameter: file");
    }
  });

  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filePath = request->getParam("file")->value();
      if (SPIFFS.exists(filePath)) {
        File downloadFile = SPIFFS.open(filePath, "r");
        AsyncWebServerResponse *response = request->beginResponse(
          "application/octet-stream",
          downloadFile.size(),
          [downloadFile](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
            return downloadFile.read(buffer, maxLen);
          }
        );
        String filename = filePath.substring(filePath.lastIndexOf('/') + 1);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        request->send(response);
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Missing parameter: file");
    }
  });



   server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "♻️ Rebooting ESP32...");
    delay(100);
    ESP.restart();
  });
  
  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
    saveNodeTable();
    NodeTable_changed = false;
    request->send(200, "text/plain", "NodeTable saved.");
  });
  
    server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/logviewer.html", "text/html");
  });

  server.on("/listlogs", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      String name = file.name();
      if (name.startsWith("log_")) {
        if (!first) json += ",";
        json += "\"" + name + "\"";
        first = false;
      }
      file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/logdata", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String file = request->getParam("file")->value();
    if (!file.startsWith("/")) file = "/" + file;

    if (!SPIFFS.exists(file)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(SPIFFS, file, "text/plain");
  });
}

String urlEncode(const String &str) {
  String encoded = "";
  char buf[4]; // for %XX + \0
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      sprintf(buf, "%%%02X", (unsigned char)c); // correct encoding like %2F
      encoded += buf;
    }
  }
  return encoded;
}

String fixPath(const String& path) {
  if (!path.startsWith("/")) {
    return "/" + path;
  }
  return path;
}

String generateFileListHTML() {
  size_t total = SPIFFS.totalBytes();
  size_t used = SPIFFS.usedBytes();
  size_t free = total - used;
  size_t freeHeap = ESP.getFreeHeap();

  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>SPIFFS Files</title></head><body>";
  html += "<a href='#' onclick=\"fetch('/reboot', {method:'POST'}).then(()=>location.reload());\"><button>Reboot ESP32</button></a><br><br>";
  html += "<h1>SPIFFS File List</h1><ul>";

  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    String fname = fixPath(file.name());
    size_t fsize = file.size();
    html += "<li>" + fname + " (" + String(fsize) + " Bytes) ";
    html += "<a href=\"/download?file=" + urlEncode(fname) + "\">[Download]</a> ";
    html += "<a href=\"/delete?file=" + urlEncode(fname) + "\" onclick=\"return confirm('Delete this file?');\">[Delete]</a>";
    html += "</li>";
    file = root.openNextFile();
  }

  html += "</ul>";
  html += "<p><b>SPIFFS Usage:</b> " + String(used) + " / " + String(total) + " Bytes used (" + String(free) + " free)</p>";
  html += "<p><b>Free RAM:</b> " + String(freeHeap) + " Bytes</p>";
  html += "<a href=\"/spiffs\">Refresh Page</a><br><br>";
  html += "<form method=\"POST\" action=\"/save_upload\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"upload\"><br><br>";
  html += "<input type=\"submit\" value=\"Upload\">";
  html += "</form>";
  html += "</body></html>";

  return html;
}




void handleRoot() {
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    String htmlContent = "<html><head><title>Tigo Data</title></head><body>";
    htmlContent += "<h1>Tigo Data</h1>"; 
    htmlContent += "<table border='1'><tr><th>No.</th><th>PV Node ID</th><th>Addr</th><th>Voltage In</th><th>Voltage Out</th><th>Duty Cycle</th><th>Current In</th><th>Watt</th><th>Temperature</th><th>Slot Counter</th><th>RSSI</th><th>Barcode</th></tr>";

    for (int i = 0; i < deviceCount; i++) {
      String barcode_nodeTable = "n/a";
      


      htmlContent += "<tr>";
      htmlContent += "<td>" + String(i+1) + "</td>";
      htmlContent += "<td>" + devices[i].pv_node_id + "</td>";
      htmlContent += "<td>" + devices[i].addr + "</td>";
      htmlContent += "<td>" + String(devices[i].voltage_in, 2) + " V</td>";
      htmlContent += "<td>" + String(devices[i].voltage_out, 2) + " V</td>";
      htmlContent += "<td>" + String(devices[i].duty_cycle, HEX) + "</td>";
      htmlContent += "<td>" + String(devices[i].current_in, 2) + " A</td>";
      int watt = round(devices[i].voltage_out * devices[i].current_in);
      htmlContent += "<td>" + String(watt) + " W</td>";
      htmlContent += "<td>" + String(devices[i].temperature, 1) + " °C</td>";
      htmlContent += "<td>" + devices[i].slot_counter + "</td>";
      htmlContent += "<td>" + String(devices[i].rssi) + "</td>";
      htmlContent += "<td>" + devices[i].barcode + "</td>";
      htmlContent += "</tr>";
    }
    htmlContent += "</table><br><br>";
    htmlContent += "<h1>Node Table</h1>";
    htmlContent += "<table border='1'><tr><th>No.</th><th>Addr</th><th>Long Address</th><th>Checksum</th></tr>";

    for (int i = 0; i < NodeTable_count; i++){
      htmlContent += "<tr>";
      htmlContent += "<td>" + String(i+1) + "</td>";
      htmlContent += "<td>" + NodeTable[i].addr + "</td>";
      htmlContent += "<td>" + NodeTable[i].longAddress + "</td>";
      htmlContent += "<td>" + NodeTable[i].checksum + "</td>";
      htmlContent += "</tr>";
    }
    htmlContent += "</table><br><br>";
    if(NodeTable_changed){
      htmlContent += "<b>The NodeTable has changed and has not been saved yet!</b>";
    }

    htmlContent += "<br><a href='/save'><button>Save NodeTable</button></a><br><br>";
    htmlContent += "</body></html>";

    request->send(200, "text/html", htmlContent);
  });
}





void handleFileUpload() {
  server.on(
    "/save_upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->redirect("/spiffs");
      //request->send(200, "text/plain", "Datei erfolgreich hochgeladen");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("Upload beginnt: %s\n", filename.c_str());
        String path = "/" + filename;
        if (SPIFFS.totalBytes() - SPIFFS.usedBytes() < len) {
          Serial.println("❌ Nicht genug Platz im SPIFFS.");
          request->send(500, "text/plain", "❌ Not enough space on SPIFFS for file: " + filename);
          return;
        }
        if (SPIFFS.exists(path)) {
          SPIFFS.remove(path);
        }
        uploadFile = SPIFFS.open(path, FILE_WRITE);
      }
      if (uploadFile) {
        uploadFile.write(data, len);
      }
      if (final) {
        Serial.printf("Upload abgeschlossen: %s (%u Bytes)\n", filename.c_str(), index + len);
        if (uploadFile) uploadFile.close();
      }
    }
  );
}
