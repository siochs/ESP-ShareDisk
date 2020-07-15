#define DATATYPE_HTML F("text/html")
#define DATATYPE_TEXT F("text/plain")
#define DATATYPE_ICON F("image/x-icon")
#define DATATYPE_STREAM F("application/octet-stream")

WebServer webServer(80);
File uploadFile;
bool isSdCardClaimed = false;

void claimSdCard()
{
    if (isSdCardClaimed)
    {
        DBG_OUTPUT_PORT.println(F("Sd card already claimed."));
        return;
    }

    if (SD.begin())
    {
        DBG_OUTPUT_PORT.println(F("Sd card claimed."));
        isSdCardClaimed = true;
        turnOnOnboadLed(); // indicate sd card is in use
        return;
    }

    DBG_OUTPUT_PORT.println(F("Something went wrong, sd card remains released."));
    isSdCardClaimed = false;
}

void releaseSdCard()
{
    if (isSdCardClaimed)
    {
        SD.end();
        isSdCardClaimed = false;
        SPI.end(); // this is not done on SD.end() !?
        DBG_OUTPUT_PORT.println(F("Sd card released."));
        return;
    }

    DBG_OUTPUT_PORT.println(F("Sd card already released."));
    return;
}

String getServerArgByName(String name)
{
    for (uint8_t index = 0; index < webServer.args(); index++)
    {
        if (webServer.argName(index).equals(name))
        {
            return webServer.arg(index);
        }
    }
    return "";
}

void deleteFileIfExists(String filePath)
{
    if (SD.exists(filePath))
    {
        SD.remove(filePath);
    }
}

String listSdCardRootFolderAsHtmlTableData()
{
    File rootFolder = SD.open(F("/"));
    String htmlTableData = "";
    while (true)
    {
        File fileEntry = rootFolder.openNextFile();
        if (!fileEntry)
        {
            break;
        }

        String fileName = fileEntry.name();
        fileName.remove(0, 1); // remove trailing slash
        String fileEntryData = "";
        if (fileEntry.isDirectory())
        {
            fileEntryData.concat(F("<td>"));
            fileEntryData.concat(fileName);
            fileEntryData.concat(F("/</td><td>-</td><td>-</td>"));
        }
        else
        {
            fileEntryData.concat(F("<td>"));
            fileEntryData.concat(fileName);
            fileEntryData.concat(F("</td><td>"));
            fileEntryData.concat(fileEntry.size());
            fileEntryData.concat(F("</td><td><a href=\"download?file="));
            fileEntryData.concat(fileName);
            fileEntryData.concat(F("\" title=\"Download\">💾</a>"));
            fileEntryData.concat(F("<a href=\"delete?file="));
            fileEntryData.concat(fileName);
            fileEntryData.concat(F("\" title=\"Delete\">🗑️</a>"));
            if (fileName != F("auto0.g") && fileName != F("BIN"))
            {
                fileEntryData.concat(F("<a href=\"autoprintonce?file="));
                fileEntryData.concat(fileName);
                fileEntryData.concat(F("\" title=\"Autoprint once on boot\">🔂</a>"));
                fileEntryData.concat(F("<a href=\"autoprintalways?file="));
                fileEntryData.concat(fileName);
                fileEntryData.concat(F("\" title=\"Always autoprint on boot\">🔁</a>"));
            }
            fileEntryData.concat(F("</td>"));
        }
        htmlTableData.concat(F("<tr>"));
        htmlTableData.concat(fileEntryData);
        htmlTableData.concat(F("</tr>"));
    }
    return htmlTableData;
}

void serveIndexView()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /"));
    File file = SPIFFS.open(F("/index.htm"), "r");
    String index_html = "";
    while (file.available())
    {
        index_html += (char)file.read();
    }
    file.close();

    if (isSdCardClaimed)
    {
        index_html.replace(F("<% sdCardNotAvailableBoxStyles %>"), F("display:none;"));
        index_html.replace(F("<% sdFilesStyles %>"), F(""));
        index_html.replace(F("<% files %>"), listSdCardRootFolderAsHtmlTableData());
    }
    else
    {
        index_html.replace(F("<% sdCardNotAvailableBoxStyles %>"), F(""));
        index_html.replace(F("<% sdFilesStyles %>"), F("display:none;"));
    }
    webServer.send(200, DATATYPE_HTML, index_html);
}

void serveFavicon()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /favicon.ico"));
    File file = SPIFFS.open(F("/favicon.ico"), "r");
    if (webServer.streamFile(file, DATATYPE_ICON) != file.size())
    {
        DBG_OUTPUT_PORT.println(F("Sent less data than expected :-\\"));
    }
    file.close();
}

void serveClaimSd()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /claimsd"));
    claimSdCard();
    webServer.sendHeader(F("Location"), F("/"));
    webServer.send(303);
}

bool sentErrorBecauseSdCardReleased()
{
    if (isSdCardClaimed)
        return false;

    webServer.send(500, DATATYPE_TEXT, F("Action not possible while sd card is released."));
    return true;
}

bool sentErrorBecauseFileArgumentsUnsatisfied()
{
    if (!webServer.hasArg(F("file")))
    {
        webServer.send(500, DATATYPE_TEXT, F("File argument required."));
        return true;
    }
    return false;
}

bool sentErrorBecauseFileDoesNotExist(String filePath)
{
    if (!SD.exists(filePath))
    {
        webServer.send(500, DATATYPE_TEXT, F("Requested file does not exist."));
        return true;
    }
    return false;
}

bool sentErrorBecauseFileNotOpenable(File file)
{
    if (!file)
    {
        webServer.send(500, DATATYPE_TEXT, F("Could not open requested file."));
        return true;
    }
    if (file.isDirectory())
    {
        webServer.send(500, DATATYPE_TEXT, F("Directories not supported."));
        return true;
    }
    return false;
}

void serveDownload()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /download"));
    bool noSdCard = sentErrorBecauseSdCardReleased();
    bool noFileArgument = sentErrorBecauseFileArgumentsUnsatisfied();
    String fileName = getServerArgByName(F("file"));
    String filePath = "/" + fileName;
    DBG_OUTPUT_PORT.print(F("Someone wants access to file "));
    DBG_OUTPUT_PORT.println(filePath);
    bool fileDoesNotExist = sentErrorBecauseFileDoesNotExist(filePath);
    File requestedFile = SD.open(filePath);
    bool fileNotOpenable = sentErrorBecauseFileNotOpenable(requestedFile);

    if (noSdCard || noFileArgument || fileDoesNotExist || fileNotOpenable)
        return;

    String attachmentHeader = F("attachment; filename=");
    attachmentHeader.concat(fileName);
    webServer.sendHeader(F("Content-Disposition"), attachmentHeader);
    webServer.streamFile(requestedFile, DATATYPE_STREAM);
    requestedFile.close();
}

void serveDelete()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /delete"));
    bool noSdCard = sentErrorBecauseSdCardReleased();
    bool noFileArgument = sentErrorBecauseFileArgumentsUnsatisfied();
    String fileName = getServerArgByName(F("file"));
    String filePath = "/" + fileName;
    DBG_OUTPUT_PORT.print(F("Someone wants to delete file "));
    DBG_OUTPUT_PORT.println(filePath);
    bool fileDoesNotExist = sentErrorBecauseFileDoesNotExist(filePath);
    File requestedFile = SD.open(filePath);
    bool fileNotOpenable = sentErrorBecauseFileNotOpenable(requestedFile);

    if (noSdCard || noFileArgument || fileDoesNotExist || fileNotOpenable)
        return;
    requestedFile.close();
    SD.remove(filePath);
    webServer.sendHeader(F("Location"), F("/"));
    webServer.send(303);
}

void handleFileUpload()
{
    bool noSdCard = sentErrorBecauseSdCardReleased();
    if (noSdCard)
        return;

    HTTPUpload &upload = webServer.upload();
    upload.filename = "/" + upload.filename;
    if (upload.status == UPLOAD_FILE_START)
    {
        DBG_OUTPUT_PORT.println(F("Begin serve request: POST /upload"));
        deleteFileIfExists(upload.filename);
        uploadFile = SD.open(upload.filename, FILE_WRITE);
        DBG_OUTPUT_PORT.print(F("Upload: START, filename: "));
        DBG_OUTPUT_PORT.println(upload.filename);
        turnOnOnboadLed();
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile)
        {
            uploadFile.write(upload.buf, upload.currentSize);
        }
        //DBG_OUTPUT_PORT.print(F("Upload: WRITE, Bytes: "));
        //DBG_OUTPUT_PORT.println(upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.close();
        }
        DBG_OUTPUT_PORT.print(F("Upload: END, Size: "));
        DBG_OUTPUT_PORT.println(upload.totalSize);
    }
}

void serveReboot()
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /reboot"));
    releaseSdCard();
    webServer.send(200, DATATYPE_TEXT, F("Device will reboot. Sd card is released."));
    sleep(1);
    hardRestart();
}

void copyFile(File srcFile, File dstFile)
{
    size_t blocks; 
    uint8_t buffer[64];
    while ((blocks = srcFile.read(buffer, sizeof(buffer))) > 0) 
    {
        dstFile.write(buffer, blocks);
    }
}

void serveAutoprint(bool printOnce=true)
{
    DBG_OUTPUT_PORT.println(F("Serve request: GET /autoprintalways"));
    bool noSdCard = sentErrorBecauseSdCardReleased();
    bool noFileArgument = sentErrorBecauseFileArgumentsUnsatisfied();
    String fileName = getServerArgByName(F("file"));
    String filePath = "/" + fileName;
    bool fileDoesNotExist = sentErrorBecauseFileDoesNotExist(filePath);
    File requestedFile = SD.open(filePath);
    bool fileNotOpenable = sentErrorBecauseFileNotOpenable(requestedFile);

    if (noSdCard || noFileArgument || fileDoesNotExist || fileNotOpenable)
        return;
    
    DBG_OUTPUT_PORT.print(F("The following file always autoprinted: "));
    DBG_OUTPUT_PORT.println(filePath);
    deleteFileIfExists(F("/auto0.g"));
    File auto0gFile = SD.open(F("/auto0.g"), "w");
    copyFile(requestedFile, auto0gFile);   
    if (printOnce)
    {
        auto0gFile.print(F("M30 /auto0.g\n"));
    }
    auto0gFile.close();
    requestedFile.close();
    webServer.sendHeader(F("Location"), F("/"));
    webServer.send(303);
}

void notFoundView()
{
    DBG_OUTPUT_PORT.println(F("Error 404 upon HTTP request"));
    webServer.send(404, DATATYPE_TEXT, F("Not found"));
}

void hookUpWebHandlers()
{
    webServer.on(F("/"), HTTP_GET, serveIndexView);
    webServer.on(F("/favicon.ico"), HTTP_GET, serveFavicon);
    webServer.on(F("/claimsd"), HTTP_GET, serveClaimSd);
    webServer.on(F("/reboot"), HTTP_GET, serveReboot);
    webServer.on(F("/download"), HTTP_GET, serveDownload);
    webServer.on(F("/delete"), HTTP_GET, serveDelete);
    webServer.on(
        F("/upload"), HTTP_POST, []() {
            webServer.sendHeader(F("Location"), F("/"));
            webServer.send(303);
        },
        handleFileUpload);
    webServer.on(F("/autoprintalways"), HTTP_GET, []() { serveAutoprint(false); });
    webServer.on(F("/autoprintonce"), HTTP_GET, []() { serveAutoprint(true); });
    webServer.onNotFound(notFoundView);
    webServer.begin();
    DBG_OUTPUT_PORT.println(F("Webserver started."));
}