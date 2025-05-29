
void setupWebserver(){
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
          // Nach Löschen zurück zur Liste
          request->redirect("/spiffs");
        } else {
          request->send(500, "text/plain", "Fehler beim Löschen der Datei");
        }
      } else {
        request->send(404, "text/plain", "Datei nicht gefunden");
      }
    } else {
      request->send(400, "text/plain", "Fehlender Parameter: file");
    }
  });

    // Route zum Herunterladen der Datei
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

      // Dateiname extrahieren (alles nach dem letzten Slash)
      String filename = filePath.substring(filePath.lastIndexOf('/') + 1);
      response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");

      request->send(response);
    } else {
      request->send(404, "text/plain", "Datei nicht gefunden");
    }
  } else {
    request->send(400, "text/plain", "Fehlender Parameter: file");
  }
});


  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
    saveNodeTable();
    NodeTable_changed = false;
    request->send(200, "text/plain", "NodeTable gespeichert.");
  });
}





String urlEncode(const String &str) {
  String encoded = "";
  char buf[4]; // für %XX + \0

  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      sprintf(buf, "%%%02X", (unsigned char)c); // korrektes %2F z. B.
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
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>SPIFFS Dateien</title></head><body>";
  html += "<h1>SPIFFS Dateiliste</h1><ul>";

  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    String fname = fixPath(file.name());
    size_t fsize = file.size();
    html += "<li>" + fname + " (" + String(fsize) + " Bytes) ";
    html += "<a href=\"/download?file=" + urlEncode(fname) + "\">[Download]</a> ";
    html += "<a href=\"/delete?file=" + urlEncode(fname) + "\" onclick=\"return confirm('Datei löschen?');\">[Löschen]</a>";
    html += "</li>";
    file = root.openNextFile();
  }

  html += "</ul>";
  html += "<a href=\"/spiffs\">Seite aktualisieren</a><br><br>";
  html += "<form method=\"POST\" action=\"/save_upload\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"upload\"><br><br>";
  html += "<input type=\"submit\" value=\"Hochladen\">";
  html += "</form>";
  html += "</body></html>";

  return html;
}




void handleRoot() {
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    String htmlContent = "<html><head><title>Tigo Data</title></head><body>";
    htmlContent += "<h1>Tigo Data</h1>"; 

    htmlContent += "<table border='1'><tr><th>Nr.</th><th>PV Node ID</th><th>Addr</th><th>Voltage In</th><th>Voltage Out</th><th>Duty Cycle</th><th>Current In</th><th>Watt</th><th>Temperature</th><th>Slot Counter</th><th>RSSI</th><th>Barcode</th></tr>";

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
      htmlContent += "<td>" + String(devices[i].temperature, 1) + " C</td>";
      htmlContent += "<td>" + devices[i].slot_counter + "</td>";
      htmlContent += "<td>" + String(devices[i].rssi) + "</td>";
      htmlContent += "<td>" + devices[i].barcode + "</td>";
      htmlContent += "</tr>";
    }
    htmlContent += "</table><br><br>";
    htmlContent += "<h1>NodeTable</h1>";
    htmlContent += "<table border='1'><tr><th>Nr.</th><th>Addr</th><th>LongAddress</th></tr>";


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
      htmlContent += "<b>Die NodeTable hat sich geändert und wurde noch nicht gespeichert!</b>";
    }
    htmlContent += "<br><a href='/save'><button>NodeTable speichern</button></a><br><br>";
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
        if (SPIFFS.exists("/" + filename)) {
          SPIFFS.remove("/" + filename);
        }
        uploadFile = SPIFFS.open("/" + filename, FILE_WRITE);
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