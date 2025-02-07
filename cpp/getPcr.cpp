#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define EIGHT_BITS 0xFF 

uint8_t addMarkerFirst(uint64_t pts, uint8_t initial) {
	uint8_t first = initial | (0xF & pts >> 29); //Get first 3 bits and add initial 
	return first | 1; //Add market_bit
}

uint8_t addMarkerSecond(uint64_t pts) {
	uint8_t second = pts >> 14 & EIGHT_BITS; //Get middle 7 bits (8th bit for market_bit)
	return second | 1; //Add marker_bit
}

uint8_t addMarkerThird(uint64_t pts) {
	return (pts & 0x7F) << 1 | 1; //Get last 7 bits and add marker_bit
}

uint64_t calculateOffset(char *input, uint64_t timestamp) {
	uint64_t result = timestamp;
	uint64_t offset = 0;
	if (input[0] == '-') {
		offset = atoll(&input[1]);
		result -= offset;
	} else {
		offset = atoll(input);
		result += offset;
	}
	return result;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("changePCR.exe  sourceTSFile\n");
		return 0;
	}

	FILE* fIn = fopen(argv[1], "rb");
//	FILE* fOut = fopen(argv[2], "wb");

    int packetCnt = 0;
	while (true) {
		unsigned char syncByte[1];
		int cnt = fread(syncByte, 1, 1, fIn);
        printf("packetCnt: %d\n", packetCnt);
		if (cnt != 1 || syncByte[0] != 0x47 || packetCnt == 25) {
			break;
		}
		unsigned char packet[188];

		fseek(fIn, -1, SEEK_CUR);

		fread(packet, 1, 188, fIn);
        packetCnt++;
		uint8_t adaptation_field_control = packet[3] & 0x30;
		uint8_t adaptation_field_length = 0;
		uint16_t offset = 0;

		//Check PTS_DTS_flags
		if (adaptation_field_control == 0x20 || adaptation_field_control == 0x30) {
			// printf("-------1-------\n");
			adaptation_field_length = packet[4];
			offset = 1 + adaptation_field_length;
			if (adaptation_field_length > 0) {
				// printf("-------2-------\n");
				uint8_t pcr_flag = (uint8_t) (packet[5] & 0x10);
				if (pcr_flag == 0x10) {
					uint64_t pcr_base = (uint64_t) ((uint64_t)packet[6] << 25 | (uint64_t)packet[7] << 17 
						| (uint64_t)packet[8] << 9 | (uint64_t)packet[9] << 1 | (uint64_t)packet[10] >> 7);
                    printf("pcr base: %lu\n", pcr_base);
//					pcr_base = (uint64_t) calculateOffset(argv[3], pcr_base);
//					packet[6] = pcr_base >> 25;
//					packet[7] = pcr_base >> 17 & EIGHT_BITS;
//					packet[8] = pcr_base >> 9 & EIGHT_BITS;
//					packet[9] = pcr_base >> 1 & EIGHT_BITS;
//					packet[10] = (pcr_base & 0x1) << 7 | (packet[10] & 0x7F);
				}
			}

		}
		// if (adaptation_field_control == 0x10 || adaptation_field_control == 0x30) {
		// 	// printf("-------4-------\n");
 		// 	offset += 4;
		// 	// Check that it's the start of a PES packet
		// 	if (packet[offset] != 0 || 
		// 		packet[offset+1] != 0 || 
		// 		packet[offset+2] != 1) {
		// 		// printf("------5---------\n");
		// 		fwrite(packet, 1, 188, fOut);
		// 		continue;
		// 	}
		// 	int streamID = packet[offset+3];
		// 	if (streamID != 188 || streamID != 190 || streamID != 191 || streamID != 240 ||
		// 		streamID != 241 || streamID != 255 || streamID != 242 || streamID != 248) {
		// 		// printf("--------6-------\n");
		// 		uint8_t ptsFlag = packet[offset+7] >> 6;
		// 		uint64_t pts = 0;

		// 		//Check PTS_DTS_flags
		// 		if (ptsFlag == 2 || ptsFlag == 3) {
		// 			pts = (uint64_t) ((uint64_t)(packet[offset+9] & 14) << 29 | (uint64_t)packet[offset+10] << 22 | 
		// 					  (uint64_t)(packet[offset+11] >> 1) << 15 | (uint64_t)packet[offset+12] << 7 | 
		// 					  (packet[offset+13] >> 1));
		// 			pts = (uint64_t) calculateOffset(argv[3], pts);

		// 			packet[offset+10] = pts >> 22 & EIGHT_BITS;
		// 			packet[offset+11] = addMarkerSecond(pts);
		// 			packet[offset+12] = pts >> 7 & EIGHT_BITS;
		// 			packet[offset+13] = addMarkerThird(pts);

		// 			if (ptsFlag == 2) {
		// 				packet[offset+9] = addMarkerFirst(pts, 0x20);
		// 			} else {
		// 				packet[offset+9] = addMarkerFirst(pts, 0x30);
		
		// 				uint64_t dts = (uint64_t) ((uint64_t)(packet[offset+14] & 14) << 29 | (uint64_t)packet[offset+15] << 22 | 
		// 					  (uint64_t)(packet[offset+16] >> 1) << 15 | (uint64_t)packet[offset+17] << 7 | 
		// 					  (packet[offset+18] >> 1));
		
		// 				dts = calculateOffset(argv[3], dts);	
		// 				packet[offset+14] = addMarkerFirst(dts, 0x10);
		// 				packet[offset+15] = dts >> 22 & EIGHT_BITS;
		// 				packet[offset+16] = addMarkerSecond(dts);
		// 				packet[offset+17] = dts >> 7 & EIGHT_BITS;
		// 				packet[offset+18] = addMarkerThird(dts);
		// 			}
		// 		}
		// 	}
		// }
//		fwrite(packet, 1, 188, fOut);
	}
	fclose(fIn);
//	fclose(fOut);
}

