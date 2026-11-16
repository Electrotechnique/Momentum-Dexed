// Janelia Array available in libraries manager
#include <Array.h>
#include "Constants.h"
#include "Utils.h"
#include <ArduinoJson.h>

FLASHMEM void deletePatch(uint8_t bank, uint8_t index);
FLASHMEM void storeVoiceInCurrentPatch(); // main.cpp: copies the engine voice into currentPatch
FLASHMEM boolean savePatch(uint8_t bankIndex, uint8_t index);

uint8_t tempBankIndex = 0;
uint8_t currentBankIndex = 0;
uint8_t currentPatchIndex = 0;
uint8_t previousBankIndex = 0;
uint8_t previousPatchIndex = 0;

PatchStruct currentPatch;

struct PatchUIDAndName
{
  uint32_t patchUID;
  String patchName;
};

typedef Array<PatchUIDAndName, PATCHES_LIMIT> Patches;

// The patch index file of a bank: one entry per patch. 1024 bytes held only 54 entries and 2048
// held 107 (measured), so banks of more patches lost index entries; 3072 holds about 160.
#define PATCH_INDEX_JSON_SIZE 3072

Patches patches;

FLASHMEM void concatBankAndUID(uint8_t bankIndex, uint32_t uid, char *result)
{
  char buf[12];
  strcpy(result, BANK_FOLDER_NAMES_SLASH[bankIndex]); // copy string one into the result.
  strcat(result, utoa(uid, buf, 10));                 // append string two to the result
}

FLASHMEM void concatBankAndFilename(uint8_t bankIndex, const char *filename, char *result)
{
  strcpy(result, BANK_FOLDER_NAMES_SLASH[bankIndex]); // copy string one into the result.
  strcat(result, filename);                           // append string two to the result
}

FLASHMEM void insertIntoPatchIndexFile(uint8_t bankIndex, uint8_t index, uint32_t UID)
{
  if (DEBUG)
    Serial.printf("insertIntoPatchIndexFile B:%d I:%d UID:%u", bankIndex, index, UID);
  if (!cardStatus)
    return;
  char result[30];
  concatBankAndFilename(bankIndex, PATCH_INDEX_FILE_NAME, result);
  DynamicJsonDocument doc(PATCH_INDEX_JSON_SIZE);

  File file = SD.open(result);
  if (!file)
  {
    if (DEBUG)
      Serial.println(F("No existing patch index file"));
  }
  else
  {
    deserializeJson(doc, file);
    if (DEBUG)
      serializeJsonPretty(doc, Serial);
  }
  file.close();
  file = SD.open(result, FILE_WRITE); // TODO FILE_WRITE_BEGIN
  file.seek(0);
  file.print("");
  doc[String(index + 1)] = UID;
  if (DEBUG)
    serializeJsonPretty(doc, Serial);
  serializeJson(doc, file);
  file.close();
}

// Creates patch index file directly from patches array for currently slected bank
// patches must be in correct order already
FLASHMEM void recreatePatchIndexFile(uint8_t bankIndex)
{
  if (!cardStatus)
    return;
  if (DEBUG)
    Serial.printf("recreatePatchIndexFile bank:%d", bankIndex);
  if (DEBUG)
    Serial.printf("recreatePatchIndexFile patches size:%d", patches.size());

  char result[30];
  concatBankAndFilename(bankIndex, PATCH_INDEX_FILE_NAME, result);
  SD.remove(result); // Delete PatchIndex file
  File file = SD.open(result, FILE_WRITE);
  DynamicJsonDocument doc(PATCH_INDEX_JSON_SIZE);
  for (uint8_t i = 0; i < patches.size(); i++)
  {
    doc[String(i + 1)] = patches[i].patchUID;
  }
  if (DEBUG)
    serializeJsonPretty(doc, Serial);
  serializeJson(doc, file);
  file.close();
}

FLASHMEM Patches getPatchesCopy()
{
  Patches p;
  for (uint8_t i = 0; i < patches.size(); i++)
  {
    p.push_back(PatchUIDAndName{patches[i].patchUID, patches[i].patchName});
  }
  return p;
}

// Patchname from a files and return it
// Returns a copy: the JsonDocument is on this function's stack, so a pointer into it was
// left dangling (and could be overwritten by an interrupt before the caller copied it)
FLASHMEM String getPatchName(File file)
{

  if (DEBUG)
    Serial.println(file.name());
  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<2048> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    if (DEBUG)
      Serial.println(F("getPatchName() - Failed to read file"));
    return String();
  }
  return String(doc["PatchName"] | "");
}

// Patchname from a UID and return it
FLASHMEM String getPatchName(uint8_t bankIndex, uint32_t uid)
{
  char result[30];
  concatBankAndUID(bankIndex, uid, result);
  if (DEBUG)
    Serial.println(result);
  File file = SD.open(result);
  if (!file)
    return MISSINGNAME;
  String patchName = getPatchName(file);
  file.close();
  return patchName;
}

// Loads Patchnames into array for display only and uses UID (filename) to recall
FLASHMEM void loadPatchNamesFromBank(uint8_t bankIndex)
{
  if (!cardStatus)
    return;
  if (DEBUG)
    Serial.println(F("LoadPatchNames from Bank") + String(bankIndex + 1));
  patches.clear();
  File bankDir = SD.open(BANK_FOLDER_NAMES_SLASH[bankIndex]);
  File patchFile;
  while (patchFile = bankDir.openNextFile())
  {
    if (!patchFile)
      break;
    if (strcmp(patchFile.name(), PATCH_INDEX_FILE_NAME) == 0)
      continue;
    if (patches.full())
    {
      if (DEBUG)
        Serial.println(F("More than max number of patches is being loaded into bank") + String(bankIndex + 1));
      patchFile.close();
      bankDir.close();
      break;
    }
    else
    {
      uint32_t uid = strtoul(patchFile.name(), NULL, 0);
      patches.push_back(PatchUIDAndName{uid, getPatchName(patchFile)});
    }
    patchFile.close();
  }

  if (patches.empty())
  {
    // Dummy patch
    patches.push_back(PatchUIDAndName{0, EMPTYNAME});
  }
  else
  {
    // Sort
    char result[30];
    concatBankAndFilename(bankIndex, PATCH_INDEX_FILE_NAME, result);
    File file = SD.open(result);
    if (file)
    {
      DynamicJsonDocument doc(PATCH_INDEX_JSON_SIZE);
      DeserializationError error = deserializeJson(doc, file);
      // file.close();
      if (error)
      {
        if (DEBUG)
          Serial.print(F("loadPatchNamesFromBank() - Deserialise error:"));
        if (DEBUG)
          Serial.println(error.c_str());
        file.close();
        return;
      }
      JsonObject obj = doc.as<JsonObject>();
      Patches patchesTemp = getPatchesCopy();
      patches.clear();
      for (JsonPair p : obj)
      {
        for (uint8_t i = 0; i < patchesTemp.size(); i++)
        {
          // Assume keys are in ascending order
          if (patchesTemp[i].patchUID == p.value())
          {
            patches.push_back(PatchUIDAndName{patchesTemp[i].patchUID, patchesTemp[i].patchName});
            break;
          }
        }
      }
      file.close();
      // Patches on the card but not in the index go at the end, and the index is written again.
      // (The index used to be written with room for only 54 patches, so larger banks lost some.)
      bool missing = false;
      for (uint8_t i = 0; i < patchesTemp.size(); i++)
      {
        bool found = false;
        for (uint8_t j = 0; j < patches.size() && !found; j++)
          found = patches[j].patchUID == patchesTemp[i].patchUID;
        if (!found && !patches.full())
        {
          patches.push_back(patchesTemp[i]);
          missing = true;
        }
      }
      if (missing)
        recreatePatchIndexFile(bankIndex);
    }
  }
}

FLASHMEM void saveBankName(String bankname)
{
  if (!cardStatus)
    return;
  DynamicJsonDocument doc(1024);

  File file = SD.open(BANKS_FILE_NAME);
  if (!file)
  {
    if (DEBUG)
      Serial.println(F("Banks file missing"));
  }
  else
  {
    deserializeJson(doc, file);
    serializeJsonPretty(doc, Serial);
  }
  file.close();
  file = SD.open(BANKS_FILE_NAME, FILE_WRITE);
  file.seek(0);
  file.print("");
  doc[String(BANK_FOLDER_NAMES[tempBankIndex])] = bankname;
  serializeJsonPretty(doc, Serial);
  serializeJson(doc, file);
  file.close();
}

FLASHMEM void checkSDCardStructure()
{
  if (!cardStatus)
    return;
  // Expecting 8 Bank folders,Sequence folder and Bankname file
  for (uint8_t i = 0; i < 8; i++)
  {
    if (!SD.exists(BANK_FOLDER_NAMES_SLASH[i]))
    {
      SD.mkdir(BANK_FOLDER_NAMES[i]);
      if (DEBUG)
        Serial.println(F("Creating Bank dir:") + String(BANK_FOLDER_NAMES[i]));
      // Save default patch file into Bank 1 folder at first index
      if (i == 0)
      {
        savePatch(0, 0);
        currentBankIndex = 0;
        currentPatchIndex = 0;
      }
    }
  }
  if (!SD.exists(SEQUENCE_FOLDER_NAME_SLASH))
  {
    SD.mkdir(SEQUENCE_FOLDER_NAME);
  }
  if (!SD.exists(PERFORMANCE_FOLDER_NAME_SLASH))
  {
    SD.mkdir(PERFORMANCE_FOLDER_NAME);
  }

  if (!SD.exists(BANKS_FILE_NAME))
  {
    File banksfile = SD.open(BANKS_FILE_NAME, FILE_WRITE);
    if (banksfile)
    {
      banksfile.println(BANKS_FILE_CONTENTS);
      banksfile.close();
    }
  }
}

FLASHMEM void loadBankNames()
{
  if (!cardStatus)
    return;
  File file = SD.open("Banknames");
  if (!file)
  {
    if (DEBUG)
      Serial.println(F("Failed to read banknames file"));
  }

  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    if (DEBUG)
      Serial.println(F("Failed to deserialise banknames file"));
    return;
  }
  // Copy values from the JsonDocument to the banknames array
  for (u_int8_t i = 0; i < 8; i++)
  {
    bankNames[i] = doc[BANK_FOLDER_NAMES[i]].as<String>();
  }
  file.close();
}

// Filename is UID
FLASHMEM boolean loadPatch(uint8_t bank, uint32_t filename)
{
  if (!cardStatus)
    return false;
  // Open file for reading - UID is Filename
  char result[30];
  concatBankAndUID(bank, filename, result);
  File file = SD.open(result);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<2048> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    if (DEBUG)
      Serial.print(F("loadPatch() - Failed to read file:"));
    if (DEBUG)
      Serial.println(filename);
    return false;
  }
  // Copy values from the JsonDocument to the PatchStruct
  // Always terminated, and a missing name (null) doesn't crash
  strlcpy(currentPatch.PatchName, doc["PatchName"] | "", sizeof(currentPatch.PatchName));
  currentPatch.UID = doc["UID"];

  // The DX7 voice, 155 bytes as hex. Patches without one (from the subtractive Momentum)
  // load as the init voice with their name and effects.
  const char *hex = doc["Voice"] | "";
  currentPatch.VoiceValid = strlen(hex) == FM_PATCH_VOICE_SIZE * 2;
  for (uint8_t i = 0; currentPatch.VoiceValid && i < FM_PATCH_VOICE_SIZE; i++)
  {
    char byteStr[3] = {hex[i * 2], hex[i * 2 + 1], 0};
    currentPatch.Voice[i] = (uint8_t)strtoul(byteStr, nullptr, 16);
  }
  currentPatch.OpEnabled = doc["OpEnabled"] | 0x3F;
  currentPatch.EngineType = min(2, doc["EngineType"] | 1); // Mark I for patches saved before it was saved
  for (uint8_t i = 0; i < 4; i++)
    currentPatch.EnvMacro[i] = min(100, doc["EnvMacro"][i] | 50);
  currentPatch.PitchBend = doc["PitchBend"] | 2;
  currentPatch.ModWheelRange = doc["ModWheelRange"] | 50;
  currentPatch.ModWheelTarget = doc["ModWheelTarget"] | 1;
  currentPatch.AftertouchRange = doc["AftertouchRange"] | 0;
  currentPatch.AftertouchTarget = doc["AftertouchTarget"] | 0;
  currentPatch.MonophonicMode = (doc["MonophonicMode"] | 0) ? 1 : 0;
  currentPatch.PortamentoTime = doc["PortamentoTime"] | 0;
  currentPatch.FilterFreq = doc["ToneCutoff"] | 127;
  currentPatch.FilterRes = doc["ToneResonance"] | 0;
  currentPatch.EnsembleEffectAmt = doc["EnsembleEffectAmt"] | 8;
  currentPatch.EnsembleEffectMix = doc["EnsembleEffectMix"] | 0;
  currentPatch.ReverbEffectTime = doc["ReverbEffectTime"] | 8;
  currentPatch.ReverbEffectMix = doc["ReverbEffectMix"] | 0;
  currentPatch.EnsembleEffectDepth = doc["EnsembleEffectDepth"] | 64;
  currentPatch.EnsembleEffectDelay = doc["EnsembleEffectDelay"] | 64;
  currentPatch.ReverbEffectDamping = doc["ReverbEffectDamping"] | 64;
  currentPatch.ReverbEffectBassCut = doc["ReverbEffectBassCut"] | 26;
  file.close();
  return true;
}

FLASHMEM void reinitialisePatch()
{
  if (DEBUG)
    Serial.println(F("Reinitialise Patch"));
  PatchStruct ps;
  currentPatch = ps;
}

FLASHMEM int8_t findPatchIndex(uint32_t UID, int8_t ignoreIndex = -1)
{
  for (uint8_t i = 0; i < patches.size(); i++)
  {
    // This is to find a duplicate UID where a patch is in the same bank multiple times
    // and the current index
    if (i == ignoreIndex)
      continue;
    // if(DEBUG) Serial.printf("findPatchIndex()  UID:%u currentUID:%u index:%i \n", UID, patches[i].patchUID, i);
    if (UID == patches[i].patchUID)
    {
      return i;
    }
  }
  if (DEBUG)
    Serial.println(F("UID not found in patch array:") + String(UID));
  return -1;
}

FLASHMEM boolean savePatch(uint8_t bankIndex, uint8_t index)
{
  if (DEBUG)
    Serial.printf("B:%d I:%d\n", bankIndex, index);
  if (!cardStatus)
    return false;
  if (patches[index].patchUID != 0 && findPatchIndex(patches[index].patchUID, index) == -1) //-1 means duplicate not found so safe to delete
  {
    // Overwriting, need to delete existing patch at this index, or it will stay on card (UID is filename)
    // but check if same patch is elsewhere in bank
    deletePatch(bankIndex, index);
  }
  patches.remove(index);
  StaticJsonDocument<2048> doc;
  doc["PatchName"] = currentPatch.PatchName;

  doc["Engine"] = "FM";
  // The DX7 voice from the engine, with the patch name as its 10 character voice name
  storeVoiceInCurrentPatch();
  for (uint8_t i = 0; i < 10; i++)
  {
    char c = currentPatch.PatchName[i];
    if (c == 0)
      memset(&currentPatch.Voice[145 + i], ' ', 10 - i);
    if (c == 0)
      break;
    currentPatch.Voice[145 + i] = (c >= 32 && c < 127) ? c : ' ';
  }
  char hex[FM_PATCH_VOICE_SIZE * 2 + 1];
  for (uint8_t i = 0; i < FM_PATCH_VOICE_SIZE; i++)
    snprintf(&hex[i * 2], 3, "%02X", currentPatch.Voice[i]);
  doc["Voice"] = hex;
  doc["OpEnabled"] = currentPatch.OpEnabled;
  doc["EngineType"] = currentPatch.EngineType;
  JsonArray envMacro = doc.createNestedArray("EnvMacro");
  for (uint8_t i = 0; i < 4; i++)
    envMacro.add(currentPatch.EnvMacro[i]);
  doc["PitchBend"] = currentPatch.PitchBend;
  doc["ModWheelRange"] = currentPatch.ModWheelRange;
  doc["ModWheelTarget"] = currentPatch.ModWheelTarget;
  doc["AftertouchRange"] = currentPatch.AftertouchRange;
  doc["AftertouchTarget"] = currentPatch.AftertouchTarget;
  doc["MonophonicMode"] = currentPatch.MonophonicMode;
  doc["PortamentoTime"] = currentPatch.PortamentoTime;
  doc["ToneCutoff"] = currentPatch.FilterFreq;
  doc["ToneResonance"] = currentPatch.FilterRes;
  doc["EnsembleEffectAmt"] = currentPatch.EnsembleEffectAmt;
  doc["EnsembleEffectMix"] = currentPatch.EnsembleEffectMix;
  doc["ReverbEffectTime"] = currentPatch.ReverbEffectTime;
  doc["ReverbEffectMix"] = currentPatch.ReverbEffectMix;
  doc["EnsembleEffectDepth"] = currentPatch.EnsembleEffectDepth;
  doc["EnsembleEffectDelay"] = currentPatch.EnsembleEffectDelay;
  doc["ReverbEffectDamping"] = currentPatch.ReverbEffectDamping;
  doc["ReverbEffectBassCut"] = currentPatch.ReverbEffectBassCut;

  // Need to generate a new UID as the patch settings may have changed if overwriting an existing patch
  String output;
  serializeJson(doc, output);      // Generate JSON without UID
  uint32_t iUID = getHash(output); // Generate UID
  currentPatch.UID = iUID;         // Give current patch a UID
  doc["UID"] = iUID;               // Insert UID
  serializeJson(doc, output);      // Generate complete JSON to save

  char result[30];
  concatBankAndUID(bankIndex, iUID, result);
  File file = SD.open(result, FILE_WRITE);
  if (!file)
  {
    if (DEBUG)
      Serial.println(F("Failed to create file:") + String(result));
    file.close();
    return false;
  }
  if (serializeJson(doc, file) == 0)
  {
    if (DEBUG)
      Serial.println(F("savePatch() - Failed to write to file"));
    file.close();
    return false;
  }
  file.close();
  // Insert into patches array
  if (patches[index].patchUID != 0)
  {
    patches[index].patchUID = iUID;
    patches[index].patchName = currentPatch.PatchName;
  }

  // Save index number to Patchindex
  insertIntoPatchIndexFile(bankIndex, index, iUID);
  loadPatchNamesFromBank(bankIndex);
  if (DEBUG)
    Serial.print(F("Saved patch:"));
  if (DEBUG)
    Serial.println(iUID);
  return true;
}

FLASHMEM void deletePatch(uint8_t bank, uint8_t index)
{
  if (!cardStatus)
    return;
  // delete from SD card
  char result[30];
  concatBankAndUID(bank, patches[index].patchUID, result);
  if (DEBUG)
    Serial.printf("Deleting Patch:%s index:%i \n", result, index);
  if (!SD.remove(result))
  {
    if (DEBUG)
      Serial.println(F("Couldn't delete from SD card"));
  }
}

FLASHMEM void deleteBank(uint8_t bank)
{
  if (!cardStatus)
    return;
  if (patches[0].patchUID == 0) // Check for empty bank
    return;
  char result[30];
  for (uint8_t index = 0; index < patches.size(); index++)
  {
    concatBankAndUID(bank, patches[index].patchUID, result);
    if (DEBUG)
      Serial.println("deleteBank:" + String(index) + "-" + String(result));
    if (!SD.remove(result))
    {
      if (DEBUG)
        Serial.println(F("Couldn't delete from SD card"));
    }
  }
  concatBankAndFilename(bank, PATCH_INDEX_FILE_NAME, result);
  if (!SD.remove(result))
  {
    if (DEBUG)
      Serial.println(F("Couldn't delete Patchindex from SD card"));
  }
}

FLASHMEM uint8_t incCurrentPatchIndex()
{
  if (currentPatchIndex < patches.size() - 1)
  {
    return ++currentPatchIndex;
  }
  else
  {
    // Go back to Start from front of array
    currentPatchIndex = 0;
    return currentPatchIndex;
  }
}

FLASHMEM uint8_t decCurrentPatchIndex()
{
  if (currentPatchIndex > 0)
  {
    return --currentPatchIndex;
  }
  else
  {
    // Go to back of array
    currentPatchIndex = patches.size() - 1;
    return currentPatchIndex;
  }
}

FLASHMEM uint8_t incCurrentBankIndex()
{
  if (currentBankIndex < 7)
  {
    return ++currentBankIndex;
  }
  else
  {
    // Go back to Start from front of array
    currentBankIndex = 0;
    return currentBankIndex;
  }
}

FLASHMEM uint8_t decCurrentBankIndex()
{
  if (currentBankIndex > 0)
  {
    return --currentBankIndex;
  }
  else
  {
    // Go to back of array
    currentBankIndex = 7;
    return currentBankIndex;
  }
}

FLASHMEM uint8_t incTempBankIndex()
{
  if (tempBankIndex < 7)
  {
    return ++tempBankIndex;
  }
  else
  {
    // Go back to Start from front of array
    tempBankIndex = 0;
    return tempBankIndex;
  }
}

FLASHMEM uint8_t decTempBankIndex()
{
  if (tempBankIndex > 0)
  {
    return --tempBankIndex;
  }
  else
  {
    // Go to back of array
    tempBankIndex = 7;
    return tempBankIndex;
  }
}