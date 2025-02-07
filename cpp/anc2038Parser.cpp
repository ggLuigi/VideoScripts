// compile this with c++14 or above
// g++ anc2038Parser.cpp -o anc2038Parser -std=c++1y
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// #define printf(...)

unsigned int get_packet_pid(unsigned char ts_packet[]) {
  // get the 13-bit pid located in (0-based) [11-19bit] [1-2byte]
  // printf("the first MSB: %d\n", convert_to_binary(ts_packet[1] & 0x1f));
  // printf("the second byte: %d\n", convert_to_binary(ts_packet[2]));
  unsigned int pid = (ts_packet[1] & 0x1f) << 8 | ts_packet[2];
  // printf("the pid in binary: %d (%d)\n", convert_to_binary(pid), pid);
  return pid;
}

int skip_adaptation_packet(unsigned char ts_packet[]) {
  int byte_index = 4;
  int adaptation_field_control = (ts_packet[3] & 0x30) >> 4;
  if (adaptation_field_control == 2) {
    // no payload
    return -1;
  } else if (adaptation_field_control ==
             3) // adaptation field present if 10 or 11
  {
    // skip the adaptation field
    // get the adaptation_length
    int adaptation_field_length = ts_packet[byte_index];
    byte_index += adaptation_field_length + 1;
    // printf("skip adaptation field length - %d\n", byte_index);
  }
  return byte_index;
}

enum SdiAncVbiDataType {
  SDI_ANC_CEA_608 = 0,    /* SMPTE 334 */
  SDI_VBI_CEA_608,        /* Line 21 */
  SDI_ANC_CEA_708,        /* SMPTE 334 */
  SDI_ANC_AFD,            /* SMPTE 2016-3 */
  SDI_ANC_Teletext_OP47,  /* OP47 */
  SDI_ANC_SCTE104,        /* SMPTE 2010 */
  SDI_ANC_VITC,           /* SMPTE 12m-2 */
  SDI_ANC_DOLBY_METADATA, /* SMPTE RDD 6 */
  SDI_VBI_VITC,           /* SMPTE 266M */
  SDI_VBI_Teletext_OP42,  /* OP42 */
  SDI_VBI_WSS,            /* EN 300 294 */
  SDI_VBI_VideoIndex,     /* RP 186 */
  SDI_VBI_WST,            /* EN 300 706 */
  SDI_ANC_SMPTE_2031,     /* SMPTE 2031 */
  SDI_VBI_MicroVideo_AFD, /* Accept360 SN17340 */
  SDI_ANC_BcastID,        /* AKA Source ID or Program Description; RP 207 */
  SDI_VBI_VPS,            /* VPS EN 300 231 */
  SDI_ANC_ARIB_B39,       /* ARIB STD B-39 video/audio mode change */
  SDI_ANC_ARIB_B37,       /* ARIB STD B-37 Closed Captions */
  SDI_ANC_PayloadID,      /* SMPTE ST-352 */
  SDI_ANC_HDR_WCG,        /* SMPTE 2108 */
  SDI_ANC_SMPTE_2038,     /* SMPTE 2038 */
  SDI_ANC_ANY = 255       /* Used by UIO SDI source */
};

struct AncPacket {
  SdiAncVbiDataType dataType;
  uint32_t lineNum;
  uint8_t DID;
  uint8_t SDID_DBN;
  uint8_t DC;
  uint8_t *pUDW;
};

static std::map<int, SdiAncVbiDataType> s_didSdidToType = {
    {0x4105, SDI_ANC_AFD},
    {0x4107, SDI_ANC_SCTE104},
    {0x4108, SDI_ANC_SMPTE_2031},
    {0x410c, SDI_ANC_HDR_WCG},
    {0x4302, SDI_ANC_Teletext_OP47},
    {0x4303, SDI_ANC_Teletext_OP47},
    {0x4501, SDI_ANC_DOLBY_METADATA},
    {0x4502, SDI_ANC_DOLBY_METADATA},
    {0x4503, SDI_ANC_DOLBY_METADATA},
    {0x4504, SDI_ANC_DOLBY_METADATA},
    {0x4505, SDI_ANC_DOLBY_METADATA},
    {0x4506, SDI_ANC_DOLBY_METADATA},
    {0x4507, SDI_ANC_DOLBY_METADATA},
    {0x4508, SDI_ANC_DOLBY_METADATA},
    {0x4509, SDI_ANC_DOLBY_METADATA},
    {0x5fdc, SDI_ANC_ARIB_B37},
    {0x5fde, SDI_ANC_ARIB_B37},
    {0x5fdf, SDI_ANC_ARIB_B37},
    {0x5ffe, SDI_ANC_ARIB_B39},
    {0x6060, SDI_ANC_VITC},
    {0x6101, SDI_ANC_CEA_708},
    {0x6102, SDI_ANC_CEA_608},
    {0x6201, SDI_ANC_BcastID}};

SdiAncVbiDataType getIsdiAncDataType(uint8_t DataID, uint8_t SecondaryDataID) {
  SdiAncVbiDataType dt =
      SDI_ANC_ANY; // returned if the did/sdid combo isn't known
  uint16_t key = uint16_t(DataID) << 8 | SecondaryDataID;
  auto search = s_didSdidToType.find(key);
  if (search != s_didSdidToType.end()) {
    dt = search->second;
  }
  return dt;
}

std::string printDataType(SdiAncVbiDataType type) {
  std::string str = "";
  switch (type) {
  case SDI_ANC_CEA_608:
    str = "SDI_ANC_CEA_608";
    break;
  case SDI_VBI_CEA_608:
    str = "SDI_VBI_CEA_608";
    break;
  case SDI_ANC_CEA_708:
    str = "SDI_ANC_CEA_708";
    break;
  case SDI_ANC_AFD:
    str = "SDI_ANC_AFD";
    break;
  case SDI_ANC_Teletext_OP47:
    str = "SDI_ANC_Teletext_OP47";
    break;
  case SDI_ANC_SCTE104:
    str = "SDI_ANC_SCTE104";
    break;
  case SDI_ANC_VITC:
    str = "SDI_ANC_VITC";
    break;
  case SDI_ANC_DOLBY_METADATA:
    str = "SDI_ANC_DOLBY_METADATA";
    break;
  case SDI_VBI_VITC:
    str = "SDI_VBI_VITC";
    break;
  case SDI_VBI_Teletext_OP42:
    str = "SDI_VBI_Teletext_OP42";
    break;
  case SDI_VBI_WSS:
    str = "SDI_VBI_WSS";
    break;
  case SDI_VBI_VideoIndex:
    str = "SDI_VBI_VideoIndex";
    break;
  case SDI_VBI_WST:
    str = "SDI_VBI_WST";
    break;
  case SDI_ANC_SMPTE_2031:
    str = "SDI_ANC_SMPTE_2031";
    break;
  case SDI_VBI_MicroVideo_AFD:
    str = "SDI_VBI_MicroVideo_AFD";
    break;
  case SDI_ANC_BcastID:
    str = "SDI_ANC_BcastID";
    break;
  case SDI_VBI_VPS:
    str = "SDI_VBI_VPS";
    break;
  case SDI_ANC_ARIB_B39:
    str = "SDI_ANC_ARIB_B39";
    break;
  case SDI_ANC_ARIB_B37:
    str = "SDI_ANC_ARIB_B37";
    break;
  case SDI_ANC_PayloadID:
    str = "SDI_ANC_PayloadID";
    break;
  case SDI_ANC_HDR_WCG:
    str = "SDI_ANC_HDR_WCG";
    break;
  case SDI_ANC_SMPTE_2038:
    str = "SDI_ANC_SMPTE_2038";
    break;
  case SDI_ANC_ANY:
    str = "SDI_ANC_ANY";
    break;
  default:
    str = "not found";
    break;
  }
  return str;
}

void getDataWordChar(char *dest, int dataCount, const uint8_t *dataWord) {
  *dest = '\0';
  int i = 0;
  while (i < dataCount) {
    /* sprintf directly to where dest points */
    sprintf(dest, "%02x ", dataWord[i]);
    i++;
    dest += 3;
  }
}

void parseUserDataWord(const unsigned char *pBuf, AncPacket pkt,
                       int byteStart) {
  for (int i = 0; i < pkt.DC; i++) {
    uint8_t dataWord = 0;
    uint8_t bitMask = 0xff;
    // ignore the first 2 MSBs
    int curBit = i * 10 + 6;
    // get bit from part of first byte
    bitMask >>= curBit % 8;
    dataWord = (uint)(pBuf[byteStart + (curBit / 8)]) & bitMask;

    // get remaining bit from next byte if needed
    int remainingBitLen = curBit % 8;
    if (remainingBitLen > 0) {
      dataWord <<= remainingBitLen;
      dataWord +=
          (uint)(pBuf[byteStart + (curBit / 8) + 1]) >> (8 - remainingBitLen);
    }
    pkt.pUDW[i] = dataWord;
  }
}

void getVancPacket(const unsigned char *pBuf, int pesSize,
                   std::vector<AncPacket> &ancPktList) {
  int idx = 0;
  //	printf("packet: size: %d\n", size);
  while (idx < pesSize) {
    if (pesSize - idx < 9) {
      // printf("idx: %d, the remaining data not matching standard with less than 9 bytes\n", idx);
      break;
    }

    // Find start code "000000"
    if (0 == (pBuf[idx] & 0xFC)) {
      AncPacket ancPacket;
      ancPacket.lineNum = ((pBuf[idx] & 0x01) << 10) + (pBuf[idx + 1] << 2) +
                          (pBuf[idx + 2] >> 6);
      ancPacket.DID = ((pBuf[idx + 3] & 0x03) << 8) + pBuf[idx + 4];
      ancPacket.SDID_DBN = (pBuf[idx + 5] << 2) + (pBuf[idx + 6] >> 6);
      ancPacket.DC =
          (((pBuf[idx + 6] & 0x3F) << 4) + (pBuf[idx + 7] >> 4)) & 0xFF;
      ancPacket.pUDW = nullptr;
      int expectedSize = 70 + ancPacket.DC * 10;
      // calculate expected size in how many bytes (8-bit)
      expectedSize = expectedSize / 8 + (expectedSize % 8 > 0 ? 1 : 0);
      if (pesSize - idx < expectedSize) {
        printf("Warn: packet ancpacket size not matching expected size: %d, idx: %d, DC: %d\n", expectedSize, idx, ancPacket.DC);
        break;
      }

      SdiAncVbiDataType dataType =
          getIsdiAncDataType(ancPacket.DID, ancPacket.SDID_DBN);
      ancPacket.dataType = dataType;
      ancPacket.pUDW = new uint8_t[ancPacket.DC];
      parseUserDataWord(pBuf, ancPacket, idx + 7);
      ancPktList.push_back(ancPacket);
      // printf("dataType: %s\n", printDataType(ancPacket.dataType).c_str());

      idx += expectedSize;
    } else {
      idx++;
    }
  }
}

bool isPayloadStart(const unsigned char *pTsPacket) {
  return (pTsPacket[1] & 0x40) != 0;
}

bool haveTS_Payload(const unsigned char *pTsPacket) {
  // adaptation_field_control: 0001 or 0011 means have payload
  return (pTsPacket[3] & 0x10) != 0;
}

bool getAdaptationField(const unsigned char *pTsPacket,
                        int &adaptation_field_len) {
  bool rv = false;
  if (pTsPacket[3] & 0x20) {
    rv = true;
    adaptation_field_len = pTsPacket[4];
  } else {
    adaptation_field_len = 0;
  }
  return rv;
}

bool getTS_Payload(const unsigned char *pTsPacket,
                   unsigned short &ts_startBytePos,
                   unsigned short &ts_payloadLen) {
  bool rv = false;
  int adaptation_field_len = 0;
  if (haveTS_Payload(pTsPacket)) {
    rv = true;
    if (getAdaptationField(pTsPacket, adaptation_field_len)) {
      ts_startBytePos = 5 + adaptation_field_len;
      ts_payloadLen = 188 - 4 - adaptation_field_len;
    } else {
      ts_startBytePos = 4;
      ts_payloadLen = 184;
    }
  }
  return rv;
}

bool isPesStart(const unsigned char *pTsPayload, unsigned short ts_payloadLen) {
  bool rv = false;
  if (ts_payloadLen >= 3 &&
      pTsPayload[0] ==
          0x00 // packet_start_code_prefix (24-bit) has to be 0x000001 to have
               // PES header that contains PTS and DTS
      && pTsPayload[1] == 0x00 && pTsPayload[2] == 0x01) {
    rv = true;
  }
  return rv;
}

bool hasPts(const unsigned char *pPesPacket) {
  unsigned char stream_id = *(pPesPacket + 3);
  // program_stream_map || padding_stream || private_stream_2 || ECM || EMM ||
  // program_stream_directory || DSMCC_stream || ITU-T Rec. H.222.1 type E
  // stream
  if (stream_id == 0xBC || stream_id == 0xBE || stream_id == 0xBF ||
      stream_id == 0xF0 || stream_id == 0xF1 || stream_id == 0xFF ||
      stream_id == 0xF2 || stream_id == 0xF8) {
    return false;
  }
  return (*(pPesPacket + 7) & 0x80) != 0;
}

int getPesPktLen(const unsigned char *pPesPacket) {
  int pes_packet_leng = (pPesPacket[4] << 8) | pPesPacket[5];
  return pes_packet_leng;
}

void getTimestamp(const unsigned char *pBuff, uint64_t &timestamp) {
  uint64_t bits32_30 = (pBuff[0] & 0x0F) >> 1;
  uint64_t bits29_15 = (pBuff[1] << 7) | (pBuff[2] & 0xFE) >> 1;
  uint64_t bits14_0 = (pBuff[3] << 7) | (pBuff[4] & 0xFE) >> 1;
  timestamp = (bits32_30 << 30) + (bits29_15 << 15) + bits14_0;
}
uint64_t getPts(unsigned char *pBuf) {
  uint64_t pts = -1;
  unsigned short ts_startBytePos = 0, ts_payloadLen = 0;
  if (isPayloadStart(pBuf)) {
    if (getTS_Payload(pBuf, ts_startBytePos, ts_payloadLen)) {
      if (isPesStart(pBuf + ts_startBytePos, ts_payloadLen)) {
        unsigned char *pPesPacket = pBuf + ts_startBytePos;
        bool gotPts = (*(pPesPacket + 7) & 0x80) != 0;
        if (hasPts(pPesPacket)) {
          // rv = true;
          unsigned char *pBuff = (pPesPacket + 9);
          getTimestamp(pBuff, pts);
          pts *= 300;
          // if (*(pPesPacket + 7) & 0x40) { // has dts
          //     getTimestamp(pBuff + 5, dts);
          //     dts *= 300;
          // } else {
          //     dts = pts;
          // }
        }
      }
    }
  }
  return pts;
}

int main(int argc, char **argv) {
  FILE *infptr, *ofptr;
  if (argc < 4) {
    printf("Usage: ./anc2038Parse <ts file> <outputType file> <vanc pid> [from "
           "packet no.] [to packet no.]\n\n");
    printf("This program will extract DID, SDID, Line number, Data count.\n");
    printf("Print all those info with packet number and the recogized "
           "AncDataType to the output text file.\n");
    printf("SDI_ANC_ANY means unrecognized data type.\n");
    return 0;
  }
  int fromPacket = -1;
  int toPacket = -1;
  if (argc == 6) {
    fromPacket = std::stoi(std::string(argv[4]));
    toPacket = std::stoi(std::string(argv[5]));
  }
  std::string input_file = argv[1];
  std::string output_file = argv[2];
  int vancPid = std::stoi(std::string(argv[3]));
  infptr = fopen(input_file.c_str(), "rb");
  ofptr = fopen(output_file.c_str(), "w");
  if (infptr == NULL || ofptr == NULL) {
    printf("Waring: cannot open file\n");
    return 0;
  }
  const int TS_BYTE_SIZE = 188;
  const int PES_HEADER_OFFSET = 14;
  unsigned char ts_packet[TS_BYTE_SIZE];
  std::vector<SdiAncVbiDataType> typeList;
  fprintf(ofptr, "packetNum\tdataType\t\tDID_SDID\tLineNum(dec)\tDataCount(dec)"
                 "\tPTS\tDataWord\n");
  int total_packet_count = -1;
  int ancPacketCnt = 0;
  if (infptr != NULL && ofptr != NULL) {
    //* fseek can read the file at specific location
    //* without looping through the big file.
    //* Otherwise, it wastes a lot of memory.
    // printf("The first seek_cur is %ld\n", ftell(infptr));
    int remainingPesBytes = 0;
    std::vector<AncPacket> ancPktList;
    uint64_t pts;
    int pesDataSize = 0;
    unsigned char *pes_packet = nullptr;
    bool pesReady = false;

    while (true) {
      unsigned char sync_byte = 0x47;
      unsigned char first_byte[1];
      int cnt = fread(first_byte, 1, 1, infptr);
      if (cnt != 1 || first_byte[0] != sync_byte ||
          (toPacket != -1 &&
           total_packet_count >= toPacket)) //|| total_packet_count >= 20)
      {
        printf("Finish reading ts file.\n");
        break;
      }
      total_packet_count++;

      // fseek: to set the read position
      // -1 to re-read the first_byte
      // start reading from the start of every packets
      fseek(infptr, -1, SEEK_CUR);
      fread(ts_packet, sizeof(ts_packet), 1, infptr);

      // do not manipulate if not in range
      if (fromPacket != -1 && total_packet_count < fromPacket) {
        continue;
      }

      unsigned int pid = get_packet_pid(ts_packet);

      if (pid == vancPid) {
        int tsPayloadStart = skip_adaptation_packet(ts_packet);
        int tsPayloadSize = TS_BYTE_SIZE - tsPayloadStart;
        if (remainingPesBytes > 0) {
          if (tsPayloadStart != -1 && tsPayloadSize > 0) {
            int idx = pesDataSize - remainingPesBytes;
            memcpy(pes_packet + idx, ts_packet + tsPayloadStart, sizeof(unsigned char) * tsPayloadSize);
            remainingPesBytes -= TS_BYTE_SIZE - tsPayloadStart;
            pesReady = (remainingPesBytes <= 0);
            // printf("pesReady? %s, remainingPesBytes: %d, pesIdx: %d\n", pesReady ? "true" : "false", remainingPesBytes, idx);
          }
        } else {
          // get new pes header
          pts = getPts(ts_packet);
          if (pts == -1) {
            continue; // not pes start
          }
          ancPktList.clear();
          int oldPesDataSize = pesDataSize;
          pesDataSize = getPesPktLen(ts_packet + tsPayloadStart) - (PES_HEADER_OFFSET - 6);
          // resize the pes_packet if the size if not enough
          if (pes_packet && pesDataSize > oldPesDataSize) {
            delete[] pes_packet;
            pes_packet = new unsigned char[pesDataSize];
          }
          if (!pes_packet) {
            pes_packet = new unsigned char[pesDataSize];
          }
          memset(pes_packet, '\0', sizeof(unsigned char) * pesDataSize);
          memcpy(pes_packet, ts_packet + tsPayloadStart + PES_HEADER_OFFSET, sizeof(unsigned char) * (tsPayloadSize - PES_HEADER_OFFSET));
          if (pesDataSize > (tsPayloadSize)) {
            remainingPesBytes = pesDataSize - (tsPayloadSize - PES_HEADER_OFFSET);
            pesReady = false;
          } else {
            pesReady = true;
          }
          // printf("packetNo: %d, tsPayloadStart: %d, tsPayloadSize: %d, pesDataSize: %d, remainingPesBytes: %d\n", total_packet_count, tsPayloadStart, tsPayloadSize, pesDataSize, remainingPesBytes);
        }
        if (pesReady) {
          getVancPacket(pes_packet,
                        pesDataSize, ancPktList);
          for (auto &vancPacket : ancPktList) {
            fprintf(ofptr, "%d\t%-22s\t%02X_%02X\t%03X(%03d)\t%02X(%03d)\t%lu",
                    total_packet_count,
                    printDataType(vancPacket.dataType).c_str(), vancPacket.DID,
                    vancPacket.SDID_DBN, vancPacket.lineNum, vancPacket.lineNum,
                    vancPacket.DC, vancPacket.DC, pts);
            char dataWordStr[vancPacket.DC * 3 + 1];
            getDataWordChar(dataWordStr, vancPacket.DC, vancPacket.pUDW);
            fprintf(ofptr, "\t%s\n", dataWordStr);
            delete[] vancPacket.pUDW;
            typeList.push_back(vancPacket.dataType);
            ancPacketCnt++;
          }
        } else {
          fprintf(ofptr, "%d,", total_packet_count);
        }
      }
    }
    delete[] pes_packet;
  }
  std::sort(typeList.begin(), typeList.end());
  auto removeDuplicate = std::unique(typeList.begin(), typeList.end());
  typeList.erase(removeDuplicate, typeList.end());
  printf("==== %d vanc packets and list of all data types ====\n",
         ancPacketCnt);
  fprintf(ofptr, "==== %d vanc packets and list of all data types ====\n",
          ancPacketCnt);
  for (auto type : typeList) {
    printf("%s\n", printDataType(type).c_str());
    fprintf(ofptr, "%s\n", printDataType(type).c_str());
  }
  fclose(infptr);
  fclose(ofptr);
  printf("Finished with %d packets\n", total_packet_count);
  return 0;
}
